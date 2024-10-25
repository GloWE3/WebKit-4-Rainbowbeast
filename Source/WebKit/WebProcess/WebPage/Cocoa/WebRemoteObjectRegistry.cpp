/*
* Copyright (C) 2013-2019 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "WebRemoteObjectRegistry.h"

#include "RemoteObjectRegistryMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebRemoteObjectRegistry);

WebRemoteObjectRegistry::WebRemoteObjectRegistry(_WKRemoteObjectRegistry *remoteObjectRegistry, WebPage& page)
    : RemoteObjectRegistry(remoteObjectRegistry)
    , m_page(page)
{
    WebProcess::singleton().addMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), page.identifier(), *this);
    page.setRemoteObjectRegistry(this);
}

WebRemoteObjectRegistry::~WebRemoteObjectRegistry()
{
    close();
}

void WebRemoteObjectRegistry::close()
{
    Ref page { m_page.get() };
    if (page->remoteObjectRegistry() == this) {
        WebProcess::singleton().removeMessageReceiver(Messages::RemoteObjectRegistry::messageReceiverName(), page->identifier());
        page->setRemoteObjectRegistry(nullptr);
    }
}

auto WebRemoteObjectRegistry::messageSender() -> MessageSender
{
    return m_page.get();
}

uint64_t WebRemoteObjectRegistry::messageDestinationID()
{
    return m_page.get().webPageProxyIdentifier().toUInt64();
}

} // namespace WebKit
