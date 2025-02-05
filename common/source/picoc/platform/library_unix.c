#include "interpreter.h"
#include "pksm_api.h"

void UnixSetupFunc() { }

/* list of all library functions and their prototypes */
struct LibraryFunction UnixFunctions[] =
{
    // gui
    { gui_choice,           "int gui_choice(char* lineOne, char* lineTwo);" },
    { gui_warn,             "void gui_warn(char* lineOne, char* lineTwo);" },
    { gui_menu6x5,          "int gui_menu_6x5(char* question, int options, char** labels, struct pkx* pokemon, enum Generation generation);" },
    { gui_menu20x2,         "int gui_menu_20x2(char* question, int options, char** labels);" },
    { gui_keyboard,         "void gui_keyboard(char* out, char* hint, int maxChars);" },
    { gui_numpad,           "void gui_numpad(unsigned int* out, char* hint, int maxDigits);" },
    { gui_boxes,            "int gui_boxes(int* fromStorage, int* box, int* slot, int doCrypt);" },
    // sav and storage
    { sav_sbo,              "int sav_sbo();" },
    { sav_gbo,              "int sav_gbo();" },
    { sav_boxDecrypt,       "void sav_box_decrypt();" },
    { sav_boxEncrypt,       "void sav_box_encrypt();" },
    { sav_get_pkx,          "void sav_get_pkx(char* data, int box, int slot);" },
    { sav_inject_pkx,       "void sav_inject_pkx(char* data, enum Generation type, int box, int slot, int doTradeEdits);" },
    { sav_get_value,        "int sav_get_value(enum SAV_Field field, ...);" },
    { sav_get_max,          "int sav_get_max(enum SAV_MaxField field, ...);" },
    { sav_set_string,       "void sav_set_string(char* string, unsigned int offset, unsigned int codepoints);" },
    { party_get_pkx,        "void party_get_pkx(char* data, int slot);" },
    { party_inject_pkx,     "void party_inject_pkx(char* data, enum Generation type, int slot);" },
    { bank_inject_pkx,      "void bank_inject_pkx(char* data, enum Generation type, int box, int slot);" },
    // pkm
    { pkx_encrypt,          "void pkx_decrypt(char* data, enum Generation type);" },
    { pkx_decrypt,          "void pkx_encrypt(char* data, enum Generation type);" },
    { pkx_box_size,         "int pkx_box_size(enum Generation gen);" },
    { pkx_party_size,       "int pkx_party_size(enum Generation gen);" },
    { pkx_generate,         "void pkx_generate(char* data, int species);" },
    { pkx_is_valid,         "int pkx_is_valid(char* data, enum Generation gen);" },
    { pkx_set_value,        "void pkx_set_value(char* data, enum Generation gen, enum PKX_Field field, ...);" },
    { pkx_get_value,        "unsigned int pkx_get_value(char* data, enum Generation gen, enum PKX_Field field, ...);" },
    // io
    { current_directory,    "char* current_directory();" },
    { read_directory,       "struct directory* read_directory(char* dir);" },
    { delete_directory,     "void delete_directory(struct directory* dir);" },
    // configurations
    { cfg_default_ot,       "char* cfg_default_ot();" },
    { cfg_default_tid,      "int cfg_default_tid();" },
    { cfg_default_sid,      "int cfg_default_sid();" },
    { cfg_default_day,      "int cfg_default_day();" },
    { cfg_default_month,    "int cfg_default_month();" },
    { cfg_default_year,     "int cfg_default_year();" },
    // networking
    { net_ip,               "char* net_ip();" },
    { net_tcp_receiver,     "int net_tcp_recv(char* buffer, int size, int* received);" },
    { net_tcp_sender,       "int net_tcp_send(char* ip, int port, char* buffer, int size);" },
    { net_udp_receiver,     "int net_udp_recv(char* buffer, int size, int* received);" },
    // i18n
    { i18n_species,         "char* i18n_species(int species);" },
    // text conversion
    { pksm_utf16_to_utf8,   "char* utf16_to_utf8(char* data);" },
    { pksm_utf8_to_utf16,   "char* utf8_to_utf16(char* data);" },
    { string_to_gen_4,      "unsigned short* string_to_gen_4(char* string);" },
    { g4_strlen,            "int g4_strlen(unsigned short* string);" },
    // end
    { NULL,                 NULL }
};

void PlatformLibraryInit(Picoc *pc)
{
    IncludeRegister(pc, "pksm.h", &UnixSetupFunc, &UnixFunctions[0],
    "struct pkx { int species; int form; };"
    "enum Generation { GEN_FOUR, GEN_FIVE, GEN_SIX, GEN_SEVEN, GEN_LGPE };"
    "struct directory { int count; char** files; };"
    "enum PKX_Field {OT_NAME, TID, SID, SHINY, LANGUAGE, MET_LOCATION, MOVE, BALL, LEVEL, GENDER,"
                    "ABILITY, IV_HP, IV_ATK, IV_DEF, IV_SPATK, IV_SPDEF, IV_SPEED, NICKNAME, ITEM,"
                    "POKERUS, EGG_DAY, EGG_MONTH, EGG_YEAR, MET_DAY, MET_MONTH, MET_YEAR, FORM,"
                    "EV_HP, EV_ATK, EV_DEF, EV_SPATK, EV_SPDEF, EV_SPEED, SPECIES, PID, NATURE, FATEFUL,"
                    "PP, PP_UPS, EGG, NICKNAMED, EGG_LOCATION, MET_LEVEL, OT_GENDER, ORIGINAL_GAME};"
    "enum SAV_Field { SAV_OT_NAME, SAV_TID, SAV_SID, SAV_GENDER, SAV_COUNTRY, SAV_SUBREGION, SAV_REGION,"
                    "SAV_LANGUAGE, SAV_MONEY, SAV_BP, SAV_HOURS, SAV_MINUTES, SAV_SECONDS, SAV_ITEM };"
    "enum SAV_MaxField { MAX_SLOTS, MAX_BOXES, MAX_WONDER_CARDS, MAX_SPECIES, MAX_MOVE, MAX_ITEM,"
                    "MAX_ABILITY, MAX_BALL, MAX_FORM };"
    "enum Pouch { NormalItem, KeyItem, TM, Mail, Medicine, Berry, Ball, Battle, Candy, ZCrystals };");
}
