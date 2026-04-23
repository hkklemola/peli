#include "cp437.h"
#include <stdint.h>

static const uint16_t cp437_to_unicode_table[256] = {
    /*  NUL    SOH    STX    ETX     EOT     ENQ     ACK     BEL      BS     TAB     LF      VT      FF      CR      SO       SI   */
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x2022, 0x25D8, 0x25CB, 0x25D9, 0x2642, 0x2640, 0x266A, 0x266B, 0x263C,
    /* 0       1       2      3       4       5       6       7       8       9       10      11      12      13      14      15   */
    
    /* DLE    DC1     DC2    DC3      DC4      NAK    SYN     ETB     CAN     EM      SUB     ESC     FS      GS      RS      US   */
    0x25BA, 0x25C4, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221F, 0x2194, 0x25B2, 0x25BC,
    /*  16     17     18      19      20      21      22      23      24      25      26      27      28      29      30      31   */
    
    /*          !      "      #       $       %       &       '       (       )       *       +       ,       -       .       /    */
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    /*  32     33     34      35      36      37      38      39      40      41      42      43      44      45      46      47   */
    
    /*  0     1       2       3       4       5       6       7       8       9       :       ;       <       =       >       ?    */
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    /*  48     49     50      51      52      53      54      55      56      57      58      59      60      61      62      63   */
    
    /*  @     A       B       C       D       E       F       G       H       I       J       K       L       M       N       O    */
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    /*  64     65     66      67      68      69      70      71      72      73      74      75      76      77      78      79   */
    
    /*  P     Q       R       S       T       U       V       W       X       Y       Z       [       \       ]       ^       _    */
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    /*  80     81     82      83      84      85      86      87      88      89      90      91      92      93      94      95   */
     
    /*  `     a        b      c       d       e       f       g       h       i       j      k       l        m       n       o    */
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    /*  96     97     98      99     100     101     102     103     104     105     106     107     108     109     110     111   */
    
    /*  p     q       r       s       t       u      v        w       x       y       z       {       |       }       ~      ⌂     */
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x2302,
    /*  112    113    114    115    116    117    118    119    120    121    122    123    124    125    126    127   */
    
    /*  Ç     ü       é       â       ä       à       å       ç       ê       ë       è       ï       î       ì       Ä      Å     */
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7, 0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    /*  128    129    130    131     132     133     134    135      136     137     138     139     140     141     142     143   */
    
    /*  É     æ       Æ       ô       ö       ò       û       ù       ÿ       Ö       Ü       ¢        £       ¥       ₧      ƒ    */
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9, 0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    /* 144    145    146     147     148     149     150     151     152     153     154     155     156     157     158     159   */
    
    /*  á     í       ó       ú       ñ       Ñ       ª       º       ¿       ⌐       ¬       ½       ¼       ¡       «        »   */
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA, 0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    /* 160    161    162     163     164     165     166     167     168     169     170     171     172      173      174    175  */
    
    /*  ░      ▒       ▓      │       ┤       ╡       ╢       ╖       ╕       ╣       ║       ╗       ╝        ╜       ╛      ┐    */
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    /* 176    177    178     179     180     181     182      183    184     185     186     187     188      189     190    191   */
    
    /*  └      ┴      ┬      ├        ─      ┼        ╞       ╟       ╚       ╔       ╩        ╦      ╠        ═       ╬      ╧    */
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F, 0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    /* 192    193    194    195      196    197       198    199     200     201      202      203    204    205      206      207 */
    
    /*  ╨      ╤      ╥       ╙        ╘      ╒      ╓         ╫      ╪        ┘      ┌       █       ▄        ▌      ▐      ▀     */
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B, 0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    /* 208    209     210    211     212     213     214     215     216     217     218     219     220     221     222     223   */
    
    /*   α      ß      Γ      π       Σ       σ       µ       τ       Φ       Θ       Ω       δ       ∞       φ       ε       ∩    */
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4, 0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    /* 224    225    226     227     228     229     230     231     232     233     234     235     236     237     238      239  */
    
    /*  ≡      ±      ≥       ≤       ⌠       ⌡       ÷       ≈       °       ∙       ·       √       ⁿ       ²       ■            */
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248, 0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
    /* 240    241    242     243      244    245      246     247     248     249     250     251     252     253     254     255  */
};
unsigned int cp437_to_unicode(unsigned char symbol)
{
    return cp437_to_unicode_table[symbol];
}

static void utf8_encode(unsigned int codepoint, char* out, size_t out_size)
{
    if(!out || out_size == 0)
        return;

    if(codepoint <= 0x7F)
    {
        out[0] = (char)codepoint;
        out[1] = '\0';
        return;
    }
    else if(codepoint <= 0x7FF)
    {
        if(out_size < 3)
        {
            out[0] = '\0';
            return;
        }
        out[0] = (char)(0xC0 | ((codepoint >> 6) & 0x1F));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        out[2] = '\0';
        return;
    }
    else
    {
        if(out_size < 4)
        {
            out[0] = '\0';
            return;
        }
        out[0] = (char)(0xE0 | ((codepoint >> 12) & 0x0F));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        out[3] = '\0';
        return;
    }
}

void cp437_to_utf8(unsigned char symbol, char* out, size_t out_size)
{
    if(!out || out_size == 0)
        return;

    unsigned int codepoint = cp437_to_unicode(symbol);
    if(codepoint == 0 && symbol != 0)
    {
        // Fallback for unmapped values.
        if(out_size >= 2)
        {
            out[0] = (char)symbol;
            out[1] = '\0';
        }
        else
        {
            out[0] = '\0';
        }
        return;
    }

    utf8_encode(codepoint, out, out_size);
}
