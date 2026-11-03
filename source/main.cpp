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

#include "ncm_mitm.hpp"

namespace ams
{
    namespace mvu
    {
        namespace
        {
            alignas(0x40) constinit u8 g_heap_memory[128_KB];
            constinit lmem::HeapHandle g_heap_handle;
            constinit bool g_heap_initialized;
            constinit os::SdkMutex g_heap_init_mutex;

            lmem::HeapHandle GetHeapHandle()
            {
                if (AMS_UNLIKELY(!g_heap_initialized))
                {
                    std::scoped_lock lk(g_heap_init_mutex);

                    if (AMS_LIKELY(!g_heap_initialized))
                    {
                        g_heap_handle = lmem::CreateExpHeap(g_heap_memory, sizeof(g_heap_memory), lmem::CreateOption_ThreadSafe);
                        g_heap_initialized = true;
                    }
                }

                return g_heap_handle;
            }
        }

        void *Allocate(size_t size)
        {
            return lmem::AllocateFromExpHeap(GetHeapHandle(), size);
        }

        void Deallocate(void *p, size_t size)
        {
            AMS_UNUSED(size);
            return lmem::FreeToExpHeap(GetHeapHandle(), p);
        }
    }

    namespace init
    {
        void InitializeSystemModule()
        {
            /* Initialize our connection to sm. */
            R_ABORT_UNLESS(sm::Initialize());
        }

        void FinalizeSystemModule()
        {
            /* ... */
        }

        void Startup()
        {
            /* ... */
        }
    }

    void NORETURN Exit(int rc)
    {
        AMS_UNUSED(rc);
        AMS_ABORT("Exit called by immortal process");
    }

    void Main()
    {
        /* Install the ncm mitm first thing: ns waits for it at boot (ncm is declared in mitm.lst).
           On unsupported firmware this releases the declaration instead. */
        static_cast<void>(mvu::LaunchNcmMitm());

        /* The mitm runs on its own thread; this thread just idles. */
        while (true)
        {
            os::SleepThread(TimeSpan::FromDays(1));
        }
    }
}

void *operator new(size_t size)
{
    return ams::mvu::Allocate(size);
}

void *operator new(size_t size, const std::nothrow_t &)
{
    return ams::mvu::Allocate(size);
}

void operator delete(void *p)
{
    return ams::mvu::Deallocate(p, 0);
}

void operator delete(void *p, size_t size)
{
    return ams::mvu::Deallocate(p, size);
}

void *operator new[](size_t size)
{
    return ams::mvu::Allocate(size);
}

void *operator new[](size_t size, const std::nothrow_t &)
{
    return ams::mvu::Allocate(size);
}

void operator delete[](void *p)
{
    return ams::mvu::Deallocate(p, 0);
}

void operator delete[](void *p, size_t size)
{
    return ams::mvu::Deallocate(p, size);
}
