/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Vitaly Chipounov, Volodymyr Kuznetsov
 *
 */

#include <iomanip>
#include <iostream>
#include <cassert>
#include "PageFault.h"

using namespace s2e::plugins;

namespace s2etools {

PageFault::PageFault(LogEvents *events, ModuleCache *mc)
{
   m_trackModule = false;
   m_connection = events->onEachItem.connect(
           sigc::mem_fun(*this, &PageFault::onItem));
   m_events = events;
   m_mc = mc;
}

PageFault::~PageFault()
{
    m_connection.disconnect();
}

void PageFault::onItem(unsigned traceIndex,
        const s2e::plugins::ExecutionTraceItemHeader &hdr,
        void *item)
{
    if (hdr.type == s2e::plugins::TRACE_PAGEFAULT) {
        PageFaultState *state = static_cast<PageFaultState*>(m_events->getState(this, &PageFaultState::factory));

        ExecutionTracePageFault *pageFault = (ExecutionTracePageFault*)item;
        if (m_trackModule) {
            ModuleCacheState *mcs = static_cast<ModuleCacheState*>(m_events->getState(m_mc, &ModuleCacheState::factory));
            const ModuleInstance *mi = mcs->getInstance(hdr.pid, pageFault->pc);
            if (!mi || mi->Name != m_module) {
                return;
            }
            state->m_totalPageFaults++;
        }else {
            state->m_totalPageFaults++;
        }
    }else

    if (hdr.type == s2e::plugins::TRACE_TLBMISS) {
        PageFaultState *state = static_cast<PageFaultState*>(m_events->getState(this, &PageFaultState::factory));

        ExecutionTracePageFault *tlbMiss = (ExecutionTracePageFault*)item;
        if (m_trackModule) {
            ModuleCacheState *mcs = static_cast<ModuleCacheState*>(m_events->getState(m_mc, &ModuleCacheState::factory));
            const ModuleInstance *mi = mcs->getInstance(hdr.pid, tlbMiss->pc);
            if (!mi || mi->Name != m_module) {
                return;
            }
            state->m_totalTlbMisses++;
        }else {
            state->m_totalTlbMisses++;
        }
    }
}

ItemProcessorState *PageFaultState::factory()
{
    return new PageFaultState();
}

PageFaultState::PageFaultState()
{
    m_totalPageFaults = 0;
    m_totalTlbMisses = 0;
}

PageFaultState::~PageFaultState()
{

}

ItemProcessorState *PageFaultState::clone() const
{
    return new PageFaultState(*this);
}


}
