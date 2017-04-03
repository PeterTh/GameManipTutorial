HRESULT WrappedID3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
  const D3D11_SUBRESOURCE_DATA *pInitialData,
  ID3D11Texture2D **ppTexture2D) {
  static UINT resW = 2560; // horizontal resolution, should be automatically determined
  static float resFactor = resW / 1600.0f; // the factor required to scale to the largest part of the pyramid

  // R11G11B10 float textures of these sizes are part of the BLOOM PYRAMID
  // Note: we do not manipulate the 50x28 buffer
  //    -- it's read by a compute shader and the whole screen white level can be off if it is the wrong size
  if(pDesc->Format == DXGI_FORMAT_R11G11B10_FLOAT) {
    if((pDesc->Width == 800 && pDesc->Height == 450)
      || (pDesc->Width == 400 && pDesc->Height == 225)
      || (pDesc->Width == 200 && pDesc->Height == 112)
      || (pDesc->Width == 100 && pDesc->Height == 56)
      /*|| (pDesc->Width == 50 && pDesc->Height == 28)*/) {
      D3D11_TEXTURE2D_DESC copy = *pDesc;
      // Scale the upper parts of the pyramid fully
      // and lower levels progressively less
      float pyramidLevelFactor = (pDesc->Width - 50) / 750.0f;
      float scalingFactor = 1.0f + (resFactor - 1.0f)*pyramidLevelFactor;
      copy.Width = (UINT)(copy.Width*scalingFactor);
      copy.Height = (UINT)(copy.Height*scalingFactor);
      pDesc = &copy;
    }
  }

  static UINT aoW = 1280;
  static UINT aoH = 720;

  // 800x450 R8G8B8A8_UNORM is the buffer used to store the AO result and subsequently blur it
  // 800x450 R32_FLOAT is used to store hierarchical Z information (individual mipmap levels are rendered to)
  //                   and serves as input to the main AO pass
  // 800x450 D24_UNORM_S8_UINT depth/stencil used together with R8G8B8A8_UNORM buffer for something (unclear) later on
  if(pDesc->Format == DXGI_FORMAT_R8G8B8A8_UNORM || pDesc->Format == DXGI_FORMAT_R32_FLOAT || pDesc->Format == DXGI_FORMAT_D24_UNORM_S8_UINT) {
    if(pDesc->Width == 800 && pDesc->Height == 450) {
      // set to our display resolution instead
      D3D11_TEXTURE2D_DESC copy = *pDesc;
      copy.Width = aoW;
      copy.Height = aoH;
      pDesc = &copy;
    }
  }

  // [...]
}

/////////////////////////////////////////

// High level description:
// IF we have 
//  - 1 viewport
//  - with the size of one of the 4 elements of the pyramid or mip levels of the AO-related buffers we changed
//  - and a primary rendertarget of type R11G11B10, R8G8B8A8_UNORM, or R32_FLOAT
//  - which is associated with a texture of a size different from the viewport
// THEN
//  - set the viewport to the texture size, or a farction of that if targeting a mipmap
//  - adjust the pixel shader constant buffer in the right slot for the current operation
void PreDraw(WrappedID3D11DeviceContext* context) {
  UINT numViewports = 0;
  context->RSGetViewports(&numViewports, NULL);
  if(numViewports == 1) {
    D3D11_VIEWPORT vp;
    context->RSGetViewports(&numViewports, &vp);

    if((vp.Width == 800 && vp.Height == 450)
      || (vp.Width == 400 && vp.Height == 225)
      || (vp.Width == 200 && vp.Height == 112)
      || (vp.Width == 100 && vp.Height == 56)
      || (vp.Width == 50 && vp.Height == 28)
      || (vp.Width == 25 && vp.Height == 14)) {

      ID3D11RenderTargetView *rtView = NULL;
      context->OMGetRenderTargets(1, &rtView, NULL);
      if(rtView) {
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        rtView->GetDesc(&desc);
        if(desc.Format == DXGI_FORMAT_R11G11B10_FLOAT || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R32_FLOAT) {
          ID3D11Resource *rt = NULL;
          rtView->GetResource(&rt);
          if(rt) {
            ID3D11Texture2D *rttex = NULL;
            rt->QueryInterface<ID3D11Texture2D>(&rttex);
            if(rttex) {
              D3D11_TEXTURE2D_DESC texdesc;
              rttex->GetDesc(&texdesc);
              if(texdesc.Width != vp.Width) {
                // Here we go!
                // Viewport is the easy part
                vp.Width = (float)texdesc.Width;
                vp.Height = (float)texdesc.Height;
                // if we are at mip slice N, divide by 2^N
                if(desc.Texture2D.MipSlice > 0) {
                  vp.Width = (float)(texdesc.Width >> desc.Texture2D.MipSlice);
                  vp.Height = (float)(texdesc.Height >> desc.Texture2D.MipSlice);
                }
                context->RSSetViewports(1, &vp);

                // The constant buffer is a bit more difficult
                // We don't want to create a new buffer every frame,
                // but we also can't use the game's because they are read-only
                // this just-in-time initialized map is a rather ugly solution,
                // but it works as long as the game only renders from 1 thread (which it does)
                // NOTE: rather than storing them statically here (basically a global) the lifetime should probably be managed
                D3D11_BUFFER_DESC buffdesc;
                buffdesc.ByteWidth = 16;
                buffdesc.Usage = D3D11_USAGE_IMMUTABLE;
                buffdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                buffdesc.CPUAccessFlags = 0;
                buffdesc.MiscFlags = 0;
                buffdesc.StructureByteStride = 16;
                D3D11_SUBRESOURCE_DATA initialdata;
                ID3D11Buffer* replacementbuffer = NULL;
                ID3D11Device* dev = NULL;

                // If we are not rendering to a mip map for hierarchical Z, the format is 
                // [ 0.5f / W, 0.5f / H, W, H ] (half-pixel size and total dimensions)
                if(desc.Texture2D.MipSlice == 0) {
                  static std::map<UINT, ID3D11Buffer*> buffers;
                  auto iter = buffers.find(texdesc.Width);
                  if(iter == buffers.cend()) {
                    float constants[4] = {0.5f / vp.Width, 0.5f / vp.Height, (float)vp.Width, (float)vp.Height};
                    context->GetDevice(&dev);
                    initialdata.pSysMem = constants;
                    dev->CreateBuffer(&buffdesc, &initialdata, &replacementbuffer);
                    buffers[texdesc.Width] = replacementbuffer;
                  }
                  context->PSSetConstantBuffers(12, 1, &buffers[texdesc.Width]);
                }
                // For hierarchical Z mips, the format is
                // [ W, H, LOD (Mip-1), 0.0f ]
                else {
                  static std::map<UINT, ID3D11Buffer*> mipBuffers;
                  auto iter = mipBuffers.find(desc.Texture2D.MipSlice);
                  if(iter == mipBuffers.cend()) {
                    float constants[4] = {vp.Width, vp.Height, (float)desc.Texture2D.MipSlice - 1, 0.0f};
                    context->GetDevice(&dev);
                    initialdata.pSysMem = constants;
                    dev->CreateBuffer(&buffdesc, &initialdata, &replacementbuffer);
                    mipBuffers[desc.Texture2D.MipSlice] = replacementbuffer;
                  }
                  context->PSSetConstantBuffers(8, 1, &mipBuffers[desc.Texture2D.MipSlice]);
                }
                if(dev) dev->Release();
              }
            }
            rt->Release();
          }
        }
        rtView->Release();
      }
    }
  }
}

void WrappedID3D11DeviceContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
  INT BaseVertexLocation) {
  if(IndexCount == 4 && StartIndexLocation == 0 && BaseVertexLocation == 0) PreDraw(this);

  // [...]
}

void WrappedID3D11DeviceContext::Draw(UINT VertexCount, UINT StartVertexLocation) {
  if(VertexCount == 4 && StartVertexLocation == 0) PreDraw(this);

  // [...]
} 
