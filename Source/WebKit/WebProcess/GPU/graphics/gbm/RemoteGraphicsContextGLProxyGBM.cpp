/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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
#include "RemoteGraphicsContextGLProxy.h"
#include "RemoteRenderingBackendProxy.h"

#if ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(GBM)

// This is a standalone check of additional requirements in this implementation,
// avoiding having to place an overwhelming amount of build guards over the rest of the code.
#if !USE(TEXTURE_MAPPER_DMABUF)
#error RemoteGraphicsContextGLProxyGBM implementation also depends on USE_TEXTURE_MAPPER_DMABUF
#endif

#include "WebProcess.h"
#include <WebCore/DMABufObject.h>
#include <WebCore/GraphicsLayerContentsDisplayDelegate.h>
#include <WebCore/TextureMapperFlags.h>
#include <WebCore/TextureMapperPlatformLayerProxyDMABuf.h>

namespace WebKit {

class DisplayDelegate final : public WebCore::GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<DisplayDelegate> create(bool isOpaque)
    {
        return adoptRef(*new DisplayDelegate(isOpaque));
    }
    virtual ~DisplayDelegate();

    void present(WebCore::DMABufObject&& dmabufObject)
    {
        m_pending = WTFMove(dmabufObject);
    }

private:
    explicit DisplayDelegate(bool isOpaque);

    // WebCore::GraphicsLayerContentsDisplayDelegate
    PlatformLayer* platformLayer() const final { return m_contentLayer.get(); }

    bool m_isOpaque { false };
    RefPtr<WebCore::TextureMapperPlatformLayerProxy> m_contentLayer;
    WebCore::DMABufObject m_pending { WebCore::DMABufObject(0) };
};

DisplayDelegate::DisplayDelegate(bool isOpaque)
    : m_isOpaque(isOpaque)
{
    m_contentLayer = WebCore::TextureMapperPlatformLayerProxyDMABuf::create(WebCore::TextureMapperPlatformLayerProxy::ContentType::WebGL);
    m_contentLayer->setSwapBuffersFunction([this](WebCore::TextureMapperPlatformLayerProxy& proxy) mutable {
        if (!!m_pending.handle) {
            Locker locker { proxy.lock() };

            OptionSet<WebCore::TextureMapperFlags> flags = WebCore::TextureMapperFlags::ShouldFlipTexture;
            if (!m_isOpaque)
                flags.add(WebCore::TextureMapperFlags::ShouldBlend);

            downcast<WebCore::TextureMapperPlatformLayerProxyDMABuf>(proxy).pushDMABuf(WTFMove(m_pending),
                [](auto&& object) { return object; }, flags);
        }
        m_pending = WebCore::DMABufObject(0);
    });
}

DisplayDelegate::~DisplayDelegate()
{
    m_contentLayer->setSwapBuffersFunction(nullptr);
}

class RemoteGraphicsContextGLProxyGBM final : public RemoteGraphicsContextGLProxy {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteGraphicsContextGLProxyGBM);
public:
    RemoteGraphicsContextGLProxyGBM(const WebCore::GraphicsContextGLAttributes&, WTF::SerialFunctionDispatcher&);
    virtual ~RemoteGraphicsContextGLProxyGBM() = default;

private:
    // WebCore::GraphicsContextGL
    RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() final;
    void prepareForDisplay() final;

    Ref<DisplayDelegate> m_layerContentsDisplayDelegate;
};

RemoteGraphicsContextGLProxyGBM::RemoteGraphicsContextGLProxyGBM(const WebCore::GraphicsContextGLAttributes& attributes, WTF::SerialFunctionDispatcher& dispatcher)
    : RemoteGraphicsContextGLProxy(attributes, dispatcher)
    , m_layerContentsDisplayDelegate(DisplayDelegate::create(!attributes.alpha))
{ }

RefPtr<WebCore::GraphicsLayerContentsDisplayDelegate> RemoteGraphicsContextGLProxyGBM::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate.copyRef();
}

void RemoteGraphicsContextGLProxyGBM::prepareForDisplay()
{
    if (isContextLost())
        return;

    auto sendResult = sendSync(Messages::RemoteGraphicsContextGL::PrepareForDisplay());
    if (!sendResult.succeeded()) {
        markContextLost();
        return;
    }

    auto& [dmabufObject] = sendResult.reply();
    m_layerContentsDisplayDelegate->present(WTFMove(dmabufObject));
}

Ref<RemoteGraphicsContextGLProxy> RemoteGraphicsContextGLProxy::platformCreate(const WebCore::GraphicsContextGLAttributes& attributes, SerialFunctionDispatcher& dispatcher)
{
    return adoptRef(*new RemoteGraphicsContextGLProxyGBM(attributes, dispatcher));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && ENABLE(WEBGL) && USE(GBM)
