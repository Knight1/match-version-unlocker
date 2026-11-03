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
#pragma once
#include <stratosphere.hpp>
#include <switch.h>

/*
 * ncm mitm for the ns process only.
 *
 * Since 20.0.0, ns copies the ProperProgramExists content meta attribute (bit 3), which it reads
 * with IContentMetaDatabase::GetAttributes, into the ApplicationDeliveryInfo used by "Match Version
 * with Local Users". A Switch 2 receiving from a Switch 1 only accepts the update when
 * ContentMetaPlatform == 0 && ProperProgramExists == 0 (switchbrew, ns cmd 2019), so Switch 1
 * consoles on 20.0.0+ are refused (error 2211-0520) for games with Switch 2 specific content.
 * We clear that bit for applications and patches as seen by ns; ns then builds and signs the
 * delivery info itself.
 *
 * Atmosphere's ncm server has MaxDomains = 0, so every ncm session is a plain session. IContentManager
 * 5 (OpenContentMetaDatabase) is therefore declared with a move handle, which is the non-domain wire
 * format of an out object. We create the session pair ourselves and register our wrapper as a mitm
 * session whose forward service is the real database, so every command we don't implement is
 * forwarded unchanged. Signatures match libstratosphere's own ncm interfaces.
 */

#define AMS_MVU_CONTENT_MANAGER_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 5, Result, OpenContentMetaDatabase, (ams::sf::OutMoveHandle out, ams::ncm::StorageId storage_id), (out, storage_id))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, IContentManagerMitmInterface, AMS_MVU_CONTENT_MANAGER_INTERFACE_INFO, 0x4D560001)

#define AMS_MVU_CONTENT_META_DATABASE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 18, Result, GetAttributes, (ams::sf::Out<u8> out_attributes, const ams::ncm::ContentMetaKey &key), (out_attributes, key))

AMS_SF_DEFINE_MITM_INTERFACE(ams::mvu, IContentMetaDatabaseMitmInterface, AMS_MVU_CONTENT_META_DATABASE_INTERFACE_INFO, 0x4D560002)

namespace ams::mvu
{
    /* ContentMetaAttribute bit 3, [20.0.0+]. Not named in libstratosphere's enum. */
    constexpr u8 ContentMetaAttribute_ProperProgramExists = (1 << 3);

    class ContentMetaDatabaseService
    {
    private:
        std::shared_ptr<::Service> m_srv;

    public:
        explicit ContentMetaDatabaseService(std::shared_ptr<::Service> srv) : m_srv(std::move(srv)) {}

    public:
        Result GetAttributes(sf::Out<u8> out_attributes, const ncm::ContentMetaKey &key);
    };
    static_assert(IsIContentMetaDatabaseMitmInterface<ContentMetaDatabaseService>);

    class ContentManagerMitmService : public sf::MitmServiceImplBase
    {
    public:
        using MitmServiceImplBase::MitmServiceImplBase;

    public:
        static bool ShouldMitm(const sm::MitmProcessInfo &client_info)
        {
            /* Only ns builds the delivery info. Every other ncm client keeps its own session. */
            return client_info.program_id == ncm::SystemProgramId::Ns;
        }

    public:
        Result OpenContentMetaDatabase(sf::OutMoveHandle out, ncm::StorageId storage_id);
    };
    static_assert(IsIContentManagerMitmInterface<ContentManagerMitmService>);

    /*
     * Installs the ncm mitm when the firmware is supported. Otherwise releases the future mitm
     * declaration from mitm.lst, so ns (and every other ncm client) is not left waiting at boot.
     * Returns whether the mitm is active.
     */
    bool LaunchNcmMitm();
}
