/*
 * match-version-unlocker
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stratosphere.hpp>
#include <stratosphere/sf/sf_mitm_dispatch.h>

#include "ncm_mitm.hpp"

namespace ams::mvu
{
    namespace
    {
        constexpr s32 ThreadPriority = 6;
        constexpr size_t ThreadStackSize = 0x4000;

        struct ServerOptions
        {
            /* Atmosphere's ncm uses 0x400; ours must be at least as large as the forward session's. */
            static constexpr size_t PointerBufferSize = 0x800;
            /* Atmosphere's ncm has MaxDomains = 0, so clients can't convert to domains either. */
            static constexpr size_t MaxDomains = 0;
            static constexpr size_t MaxDomainObjects = 0;
            static constexpr bool CanDeferInvokeRequest = false;
            static constexpr bool CanManageMitmServers = true;
        };

        /* ns's ncm session(s) plus one session per content meta database ns keeps open. */
        constexpr size_t MaxSessions = 40;

        class ServerManager final : public sf::hipc::ServerManager<1, ServerOptions, MaxSessions>
        {
        private:
            virtual ams::Result OnNeedsToAccept(int port_index, Server *server) override;
        };

        ServerManager g_server_manager;

        Result ServerManager::OnNeedsToAccept(int port_index, Server *server)
        {
            AMS_UNUSED(port_index);
            std::shared_ptr<::Service> fsrv;
            sm::MitmProcessInfo client_info;
            server->AcknowledgeMitmSession(std::addressof(fsrv), std::addressof(client_info));
            return this->AcceptMitmImpl(server, sf::CreateSharedObjectEmplaced<IContentManagerMitmInterface, ContentManagerMitmService>(decltype(fsrv)(fsrv), client_info), fsrv);
        }

        alignas(os::MemoryPageSize) u8 g_thread_stack[ThreadStackSize];
        os::ThreadType g_thread;

        void LoopServerThread(void *)
        {
            g_server_manager.LoopProcess();
        }

        constexpr sm::ServiceName NcmServiceName = sm::ServiceName::Encode("ncm");

        /* Active on 19.0.0 and newer, with no upper limit, so a new firmware does not need a new
           build. patches/ keeps libstratosphere from aborting on firmware it does not know yet.
           ProperProgramExists was added in 20.0.0: on 19.x the module most likely has nothing to
           change, but it does no harm there either. */
        bool IsSupportedFirmware()
        {
            return hos::GetVersion() >= hos::Version_19_0_0;
        }

        bool IsApplicationOrPatch(const ncm::ContentMetaKey &key)
        {
            return key.type == ncm::ContentMetaType::Application || key.type == ncm::ContentMetaType::Patch;
        }

        /* Wraps a real IContentMetaDatabase session in our own mitm session. On any failure the real
           handle is handed to ns untouched. */
        os::NativeHandle WrapContentMetaDatabase(const ::Service &real)
        {
            auto released = std::make_shared<bool>(false);
            std::shared_ptr<::Service> fsrv(new ::Service(real), [released](::Service *s) {
                if (!*released)
                {
                    serviceClose(s);
                }
                delete s;
            });

            os::NativeHandle server_handle, client_handle;
            if (R_SUCCEEDED(sf::hipc::CreateSession(std::addressof(server_handle), std::addressof(client_handle))))
            {
                sf::SharedPointer<IContentMetaDatabaseMitmInterface> obj = sf::CreateSharedObjectEmplaced<IContentMetaDatabaseMitmInterface, ContentMetaDatabaseService>(fsrv);
                if (R_SUCCEEDED(g_server_manager.RegisterMitmSession(server_handle, sf::cmif::ServiceObjectHolder(std::move(obj)), std::move(fsrv))))
                {
                    return client_handle;
                }
                os::CloseNativeHandle(server_handle);
                os::CloseNativeHandle(client_handle);
            }

            *released = true;
            return real.session;
        }
    }

    bool LaunchNcmMitm()
    {
        if (!IsSupportedFirmware())
        {
            static_cast<void>(sm::mitm::ClearFutureMitm(NcmServiceName));
            return false;
        }

        if (R_FAILED(g_server_manager.RegisterMitmServer<ContentManagerMitmService>(0, NcmServiceName)))
        {
            static_cast<void>(sm::mitm::ClearFutureMitm(NcmServiceName));
            return false;
        }

        R_ABORT_UNLESS(os::CreateThread(&g_thread, LoopServerThread, nullptr, g_thread_stack, ThreadStackSize, ThreadPriority));
        os::SetThreadNamePointer(&g_thread, "mvu::NcmMitm");
        os::StartThread(&g_thread);
        return true;
    }

    Result ContentManagerMitmService::OpenContentMetaDatabase(sf::OutMoveHandle out, ncm::StorageId storage_id)
    {
        /* Sub-objects of a domain session can't be wrapped this way; leave them untouched. */
        if (serviceIsDomain(m_forward_service.get()))
        {
            R_THROW(sm::mitm::ResultShouldForwardToSession());
        }

        const u8 storage = static_cast<u8>(storage_id);
        ::Service real;
        const Result rc = serviceMitmDispatchIn(m_forward_service.get(), 5, storage, .out_num_objects = 1, .out_objects = &real);
        R_TRY(rc);

        out.SetValue(WrapContentMetaDatabase(real), false);
        R_SUCCEED();
    }

    Result ContentMetaDatabaseService::GetAttributes(sf::Out<u8> out_attributes, const ncm::ContentMetaKey &key)
    {
        u8 attributes = 0;
        const Result rc = serviceMitmDispatchInOut(m_srv.get(), 18, key, attributes);
        if (R_SUCCEEDED(rc) && IsApplicationOrPatch(key))
        {
            attributes = static_cast<u8>(attributes & ~ContentMetaAttribute_ProperProgramExists);
        }
        out_attributes.SetValue(attributes);
        return rc;
    }
}
