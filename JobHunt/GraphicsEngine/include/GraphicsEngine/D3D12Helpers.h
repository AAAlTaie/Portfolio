#pragma once
#include <stdexcept>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
inline void ThrowIfFailed(HRESULT hr, const char* msg=nullptr){
    if(FAILED(hr)){
        if(msg) throw std::runtime_error(std::string(msg));
        throw std::runtime_error("HRESULT failed");
    }
}
template<typename T> using ComPtr = Microsoft::WRL::ComPtr<T>;
