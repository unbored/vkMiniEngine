#include "TextureManager.h"
#include "CommandContext.h"
#include "FileUtility.h"
#include "GraphicsCommon.h"
#include "GraphicsCore.h"
#include <map>
#include <thread>

using namespace std;
using namespace Graphics;
using Utility::ByteArray;

//
// A ManagedTexture allows for multiple threads to request a Texture load of the same
// file.  It also contains a reference count of the Texture so that it can be freed
// when it is no longer referenced.
//
// Raw ManagedTexture pointers are not exposed to clients.
//
class ManagedTexture : public Texture
{
    friend class TextureRef;

public:
    ManagedTexture(const string &FileName);

    void WaitForLoad(void) const;
    void CreateFromMemory(ByteArray memory, eDefaultTexture fallback, bool sRGB);

private:
    bool IsValid(void) const { return m_IsValid; }
    void Unload();

    std::string m_MapKey; // For deleting from the map later
    bool m_IsValid;
    bool m_IsLoading;
    size_t m_ReferenceCount;
};

namespace TextureManager
{
string s_RootPath = "";
map<string, std::unique_ptr<ManagedTexture>> s_TextureCache;

void Initialize(const string &TextureLibRoot) { s_RootPath = TextureLibRoot; }

void Shutdown(void)
{
    for (auto &i : s_TextureCache)
    {
        i.second->Destroy();
    }
    s_TextureCache.clear();
}

mutex s_Mutex;

ManagedTexture *FindOrLoadTexture(const string &fileName, eDefaultTexture fallback, bool forceSRGB)
{
    ManagedTexture *tex = nullptr;

    {
        lock_guard<mutex> Guard(s_Mutex);

        string key = fileName;
        if (forceSRGB)
            key += "_sRGB";

        // Search for an existing managed texture
        auto iter = s_TextureCache.find(key);
        if (iter != s_TextureCache.end())
        {
            // If a texture was already created make sure it has finished loading before
            // returning a point to it.
            tex = iter->second.get();
            tex->WaitForLoad();
            return tex;
        }
        else
        {
            // If it's not found, create a new managed texture and start loading it
            tex = new ManagedTexture(key);
            s_TextureCache[key].reset(tex);
        }
    }

    Utility::ByteArray ba = Utility::ReadFileSync(s_RootPath + fileName);
    tex->CreateFromMemory(ba, fallback, forceSRGB);

    // This was the first time it was requested, so indicate that the caller must read the file
    return tex;
}

void DestroyTexture(const string &key)
{
    lock_guard<mutex> Guard(s_Mutex);

    auto iter = s_TextureCache.find(key);
    if (iter != s_TextureCache.end())
    {
        iter->second->Destroy();
        s_TextureCache.erase(iter);
    }
}

} // namespace TextureManager

ManagedTexture::ManagedTexture(const string &FileName)
    : m_MapKey(FileName), m_IsValid(false), m_IsLoading(true), m_ReferenceCount(0)
{
    m_ImageView = nullptr;
}

void ManagedTexture::CreateFromMemory(ByteArray ba, eDefaultTexture fallback, bool forceSRGB)
{
    if (ba->size() == 0)
    {
        m_ImageView = GetDefaultTexture(fallback);
    }
    else
    {
        if (CreateKTXFromMemory((const uint8_t *)ba->data(), ba->size(), forceSRGB))
        {
            m_IsValid = true;
        }
        else
        {
            // TODO: Why is here a Descriptor Copy?
            // g_Device->CopyDescriptorsSimple(1, m_hCpuDescriptorHandle,
            // GetDefaultTexture(fallback),
            // D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            m_ImageView = GetDefaultTexture(fallback);
        }
    }

    m_IsLoading = false;
}

void ManagedTexture::WaitForLoad(void) const
{
    while ((volatile bool &)m_IsLoading)
        this_thread::yield();
}

void ManagedTexture::Unload() { TextureManager::DestroyTexture(m_MapKey); }

TextureRef::TextureRef(const TextureRef &ref) : m_ref(ref.m_ref)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_ref != nullptr)
    {
        ++m_ref->m_ReferenceCount;
    }
}

TextureRef::TextureRef(ManagedTexture *tex) : m_ref(tex)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_ref != nullptr)
        ++m_ref->m_ReferenceCount;
}

TextureRef::~TextureRef()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_ref != nullptr && --m_ref->m_ReferenceCount == 0)
    {
        m_ref->Unload();
        m_ref = nullptr;
    }
}

void TextureRef::operator=(std::nullptr_t)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_ref != nullptr)
        --m_ref->m_ReferenceCount;

    m_ref = nullptr;
}

void TextureRef::operator=(TextureRef &rhs)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (m_ref != nullptr)
        --m_ref->m_ReferenceCount;

    m_ref = rhs.m_ref;

    if (m_ref != nullptr)
        ++m_ref->m_ReferenceCount;
}

bool TextureRef::IsValid() const { return m_ref && m_ref->IsValid(); }

const Texture *TextureRef::Get(void) const { return m_ref; }

const Texture *TextureRef::operator->(void) const
{
    ASSERT(m_ref != nullptr);
    return m_ref;
}

TextureRef TextureManager::LoadKTXFromFile(const string &filePath, eDefaultTexture fallback, bool forceSRGB)
{
    return FindOrLoadTexture(filePath, fallback, forceSRGB);
}
