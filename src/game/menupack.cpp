// game/menupack.cpp -- the resident menu pack and its sound player, and the
// stage's MessageTable (game/menupack.h).

#include "game/menupack.h"

#include <string.h>

#include "game/fileloader.h"
#include "game/seplayer.h"

static const char kMenuPackPath[]    = "dataEU/packmenuEU.pak";
static const char kMenuSeTablePath[] = "../data/sound/se/seMenu_SE.seh";

static PackFile *sMenuPack;         /* sbss 0x31ae98 (gp-0x79d8) */
static u_char    sMenuPackLoaded;   /* sbss 0x31ae9c (gp-0x79d4) */
static SePlayer *sSePlayer;         /* sbss 0x31aea0 (gp-0x79d0) */
static u_char    sSePlayerLoaded;   /* sbss 0x31aea4 (gp-0x79cc) */

// Inline in the ROM (createMenuSePlayer).
static void *menuPackData(const char *name)
{
    return sMenuPackLoaded ? sMenuPack->getData(name) : nullptr;
}

static int menuPackSize(const char *name)
{
    return sMenuPackLoaded ? sMenuPack->getSize(name) : 0;
}

// An empty file name counts as none (inline in the ROM).
static const char *bankFileName(const char *file)
{
    return file[0] != '\0' ? file : nullptr;
}

// FS2 0x002c3d30
int getMenuPackItemSize(int id)
{
    return sMenuPackLoaded ? sMenuPack->getItemSize(id) : 0;
}

// FS2 0x002c3d70
void *getMenuPackItem(int id)
{
    return sMenuPackLoaded ? sMenuPack->getItem(id) : nullptr;
}

// callcheck: ~SePlayer (the delete-expression)
// FS2 0x002c3db0
void unloadMenuPack()
{
    delete sMenuPack;
    sMenuPack = nullptr;
    sMenuPackLoaded = 0;
    delete sSePlayer;
    sSePlayer = nullptr;
    sSePlayerLoaded = 0;
}

// The player, its table and every bank of it, from the pack.
// callcheck: operator_new (the new-expression)
// FS2 0x002c3eb0
static void createMenuSePlayer()
{
    if (sSePlayerLoaded == 1)
        return;
    sSePlayer = new SePlayer();
    sSePlayer->setTable(static_cast<SeTable *>(menuPackData(kMenuSeTablePath)));
    const SeTable *table = sSePlayer->m_table;
    int bankCount = table != nullptr ? table->bankCount() : 0;
    for (int bank = 0; bank < bankCount; bank++) {
        const SeBankInfo &info = table->banks[bank];
        void *file = menuPackData(bankFileName(info.file));
        if (info.isStream) {
            const char *streamFile = bankFileName(info.streamFile);
            void *body = menuPackData(streamFile);
            sSePlayer->openStreamBank(bank, file, body, menuPackSize(streamFile));
        }
        else {
            sSePlayer->loadBank(bank, file);
        }
    }
    sSePlayerLoaded = 1;
}

// callcheck: operator_new (the new-expression)
// FS2 0x002c3e10
void loadMenuPack()
{
    if (sMenuPackLoaded == 1)
        return;
    delete sMenuPack;
    PackFile *pack = new PackFile(kMenuPackPath, 0);
    pack->load(0, false);
    sMenuPack = pack;
    sMenuPackLoaded = 1;
    createMenuSePlayer();
}

// FS2 0x002c4140
SePlayer *getSePlayer()
{
    return sSePlayer;
}

// FS2 0x002c4150
const char *MessageTable::find(const char *key) const
{
    if (m_file == nullptr)
        return nullptr;
    for (u_int i = 0; i < m_file->count; i++) {
        const MessageRecord &record = m_file->records[i];
        if (strcmp(key, record.key) == 0)
            return record.text;
    }
    return nullptr;
}

// FS2 0x002c41f0
bool MessageTable::setFile(void *file)
{
    m_file = nullptr;
    MessageFile *messages = static_cast<MessageFile *>(file);
    if (messages == nullptr || messages->magic != kMessageFileMagic)
        return false;
    m_file = messages;
    if (messages->relocated == 0) {
        messages->relocated = 1;
        char *base = reinterpret_cast<char *>(messages);
        for (u_int i = 0; i < messages->count; i++) {
            MessageRecord &record = messages->records[i];
            record.key.set(base + record.key.rel);
            record.text.set(base + record.text.rel);
        }
    }
    return true;
}

// FS2 0x002c4290
MessageTable::MessageTable()
    : m_file(nullptr)
{
}
