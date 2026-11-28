#include "PresetStore.h"

#include "SettingsFields.h"

#include <algorithm>

namespace
{
    constexpr int kMaxNameLength = 60;
    constexpr juce::int64 kMaxFileSize = 4 * 1024 * 1024;   // a preset is a few KB; anything bigger is not one

    bool validSlot (int slot) noexcept   { return slot >= 1 && slot <= PresetStore::kSlots; }

    juce::String cleanName (const juce::String& name)
    {
        auto s = name.removeCharacters ("\r\n\t").trim();

        if (s.length() > kMaxNameLength)
            s = s.substring (0, kMaxNameLength).trimEnd();

        return s.isEmpty() ? juce::String ("Preset") : s;
    }

    /** The slot from the leading digits of a file name, 0 if there are none or it is out of range. */
    int slotFromFileName (const juce::String& fileName)
    {
        const auto digits = fileName.initialSectionContainingOnly ("0123456789");

        if (digits.isEmpty() || digits.length() > 9)
            return 0;

        const int slot = digits.getIntValue();
        return validSlot (slot) ? slot : 0;
    }

    juce::String nameFromFileName (const juce::File& file)
    {
        auto s = file.getFileNameWithoutExtension();
        s = s.substring (s.initialSectionContainingOnly ("0123456789").length());
        return cleanName (s);
    }

    bool readJson (const juce::File& file, juce::var& result)
    {
        if (! file.existsAsFile() || file.getSize() > kMaxFileSize)
            return false;

        return juce::JSON::parse (file.loadFileAsString(), result).wasOk() && result.isObject();
    }

    juce::String fileNameFor (int slot, const juce::String& name)
    {
        auto legal = juce::File::createLegalFileName (name).trim();

        // A name that is only dots would read as a hidden or special file.
        if (legal.isEmpty() || legal.containsOnly ("."))
            legal = "Preset";

        return juce::String (slot).paddedLeft ('0', 3) + " " + legal + ".json";
    }
}

//==============================================================================
PresetStore::PresetStore (const juce::File& folder)
    : dir (folder)
{
    refresh();
}

void PresetStore::refresh()
{
    list.clear();

    if (! dir.isDirectory())
        return;

    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");

    // The file whose name sorts first wins a slot that two files claim.
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        const int c = a.getFileName().compareIgnoreCase (b.getFileName());
        return c != 0 ? c < 0 : a.getFileName().compare (b.getFileName()) < 0;
    });

    std::vector<bool> taken ((size_t) kSlots + 1, false);

    for (const auto& file : files)
    {
        // Hidden files include the temporary copy of an interrupted save.
        if (file.getFileName().startsWithChar ('.'))
            continue;

        int slot = 0;
        juce::String name;

        juce::var doc;
        if (readJson (file, doc))
        {
            const auto s = doc["slot"];
            if (s.isInt() || s.isInt64() || s.isDouble())
            {
                const double d = (double) s;
                if (d >= 1.0 && d <= (double) kSlots)
                    slot = (int) d;
            }

            if (doc["name"].isString())
                name = cleanName (doc["name"].toString());
        }

        if (! validSlot (slot))
            slot = slotFromFileName (file.getFileName());

        if (! validSlot (slot) || taken[(size_t) slot])
            continue;

        if (name.isEmpty())
            name = nameFromFileName (file);

        taken[(size_t) slot] = true;
        list.push_back ({ slot, name, file });
    }

    std::sort (list.begin(), list.end(), [] (const Entry& a, const Entry& b) { return a.slot < b.slot; });
}

const PresetStore::Entry* PresetStore::find (int slot) const noexcept
{
    for (const auto& e : list)
        if (e.slot == slot)
            return &e;

    return nullptr;
}

//==============================================================================
bool PresetStore::load (int slot, vm::Settings& into) const
{
    const auto* e = find (slot);
    if (e == nullptr)
        return false;

    juce::var doc;
    if (! readJson (e->file, doc))
        return false;

    const auto settings = doc["settings"];
    if (! settings.isObject())
        return false;

    // Defaults for whatever an older file does not know.
    vm::Settings loaded;
    loaded.enabled = into.enabled;
    loaded.freezeSolo = into.freezeSolo;
    loaded.freezeHarmony = into.freezeHarmony;

    fields::fromVar (settings, loaded);
    into = loaded;
    return true;
}

bool PresetStore::write (int slot, const juce::String& name, const juce::var& settings)
{
    if (! validSlot (slot) || ! dir.createDirectory().wasOk())
        return false;

    const auto cleaned = cleanName (name);

    juce::DynamicObject::Ptr doc = new juce::DynamicObject();
    doc->setProperty ("format", 1);
    doc->setProperty ("slot", slot);
    doc->setProperty ("name", cleaned);
    doc->setProperty ("settings", settings);

    const auto target = dir.getChildFile (fileNameFor (slot, cleaned));

    // Copied: refresh() below invalidates the entry.
    juce::File old;
    if (const auto* e = find (slot))
        old = e->file;

    // The new file is complete on disk before the old one goes, so a failure leaves a preset, never none.
    bool ok = target.replaceWithText (juce::JSON::toString (juce::var (doc.get()), false, 6), false, false, "\n");

    // Compared as File, so a name that only differs in case (same file on macOS) is not deleted.
    if (ok && old != juce::File() && old != target && old.existsAsFile())
        ok = old.deleteFile();

    refresh();
    return ok;
}

bool PresetStore::save (int slot, const juce::String& name, const vm::Settings& settings, bool keepUnknown)
{
    juce::var fresh = fields::toVar (settings);

    if (keepUnknown)
    {
        juce::var doc;
        if (const auto* e = find (slot); e != nullptr && readJson (e->file, doc))
            if (const auto* before = doc["settings"].getDynamicObject())
                if (auto* now = fresh.getDynamicObject())
                    for (const auto& p : before->getProperties())
                        if (! now->hasProperty (p.name))
                            now->setProperty (p.name, p.value);
    }

    return write (slot, name, fresh);
}

bool PresetStore::rename (int slot, const juce::String& newName)
{
    const auto* e = find (slot);
    if (e == nullptr)
        return false;

    // The settings are carried over as written, including keys this version does not know.
    juce::var doc;
    if (! readJson (e->file, doc) || ! doc["settings"].isObject())
        return false;

    return write (slot, newName, doc["settings"]);
}

bool PresetStore::patch (int slot, const juce::NamedValueSet& keys, juce::NamedValueSet* previous)
{
    const auto* e = find (slot);
    if (e == nullptr)
        return false;

    juce::var doc;
    if (! readJson (e->file, doc))
        return false;

    auto* settings = doc["settings"].getDynamicObject();
    if (settings == nullptr)
        return false;

    juce::NamedValueSet old;
    for (const auto& key : keys)
    {
        old.set (key.name, settings->getProperty (key.name));   // void if it was not there
        if (key.value.isVoid())
            settings->removeProperty (key.name);
        else
            settings->setProperty (key.name, key.value);
    }

    // Copied: write() refreshes the list, which invalidates the entry.
    const auto name = e->name;
    if (! write (slot, name, doc["settings"]))
        return false;

    if (previous != nullptr)
        *previous = old;
    return true;
}

bool PresetStore::remove (int slot)
{
    const auto* e = find (slot);
    if (e == nullptr)
        return false;

    const auto file = e->file;
    const bool gone = ! file.existsAsFile() || file.moveToTrash() || file.deleteFile();

    refresh();
    return gone;
}

//==============================================================================
int PresetStore::firstFreeSlot() const noexcept
{
    int expected = 1;

    for (const auto& e : list)
    {
        if (e.slot > expected)
            break;
        expected = e.slot + 1;
    }

    return expected <= kSlots ? expected : 0;
}

int PresetStore::next (int slot) const noexcept
{
    for (const auto& e : list)
        if (e.slot > slot)
            return e.slot;

    return list.empty() ? 0 : list.front().slot;
}

int PresetStore::previous (int slot) const noexcept
{
    for (auto it = list.rbegin(); it != list.rend(); ++it)
        if (it->slot < slot)
            return it->slot;

    return list.empty() ? 0 : list.back().slot;
}

juce::String PresetStore::label (int slot, const juce::String& name)
{
    return juce::String (slot).paddedLeft ('0', 3) + " " + juce::String::fromUTF8 ("\xe2\x80\x93") + " " + name;
}
