/* UAX 14 class-pair matrices through AnalyzeLineBreakpoints.
 *
 * For every pair of line-break classes (one representative character each) the text "X Y", "X SP Y" and "X CM Y" is analysed and
 * the boundary conditions are compared with the expectation derived from the UAX 14 rules in rule order (LB4..LB31).  The condition
 * after a character and the condition before the next one describe the same boundary and must agree (verified in the
 * decomp: ClassPairsToBreakpoints copies the "after" bits of position i into the "before" bits of position i + 1).
 * Expectation letters: x = no break (MAY_NOT_BREAK), / = break allowed (CAN_BREAK), ! = mandatory (MUST_BREAK).
 * Table cells end in H (hard check) or S (soft: rules whose exact form varies with the Unicode version, or contextual CM rules;
 * a mismatch is recorded with t_note only).  Generated from the rule order, see the header comment of each table. */
#include "ms.h"
#define MT(n) TEST(r2_bidi_linebreak_matrix_##n)

/* generated pair tables: expected boundary conditions per UAX 14 rule order for the class representatives below.
   Characters: BK=0x2028 CR=0x000d LF=0x000a NL=0x0085 SP=0x0020 ZW=0x200b WJ=0x2060 GL=0x00a0 CL=0x007d OP=0x0028 QU=0x0022 EX=0x0021 IS=0x002c NU=0x0035 SY=0x002f AL=0x0061 ID=0x4e2d HY=0x002d BA=0x2010 CM=0x0301 */
#define LBN 20
static const char *const lb_name[LBN] = {"BK","CR","LF","NL","SP","ZW","WJ","GL","CL","OP","QU","EX","IS","NU","SY","AL","ID","HY","BA","CM"};
static const WCHAR lb_ch[LBN] = {0x2028,0x000d,0x000a,0x0085,0x0020,0x200b,0x2060,0x00a0,0x007d,0x0028,0x0022,0x0021,0x002c,0x0035,0x002f,0x0061,0x4e2d,0x002d,0x2010,0x0301};
static const char lb_pair[LBN][LBN][4] = {
  {"!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!S"},
  {"!H","!H","xH","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!S"},
  {"!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!S"},
  {"!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!H","!S"},
  {"xH","xH","xH","xH","xH","xH","xH","/H","xH","/H","/H","xH","xH","/H","xH","/H","/H","/H","/H","/S"},
  {"xH","xH","xH","xH","xH","xH","/H","/H","/H","/H","/H","/H","/H","/H","/H","/H","/H","/H","/H","/S"},
  {"xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","/H","xH","xH","xH","/H","xH","/H","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","xS","xH","xH","xH","xH","xH","xH","xH","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","/H","xH","xH","xH","/H","xH","/H","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","/H","xH","xH","xH","xS","xH","xS","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","xS","xH","xH","xH","xS","xH","xH","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","/H","xH","xH","xH","xS","xH","/H","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","xS","xH","xH","xH","xH","xH","xH","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","xS","xH","/H","xH","xH","xH","/H","xH","/H","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","/H","xH","/H","xH","xH","xH","xS","xH","/H","/H","xH","xH","xS"},
  {"xH","xH","xH","xH","xH","xH","xH","/H","xH","/H","xH","xH","xH","/H","xH","/H","/H","xH","xH","xS"},
  {"xS","xS","xS","xS","xS","xS","xS","xS","xS","xS","xS","xS","xS","xS","xS","xS","/S","xS","xS","xS"}
};
static const char lb_trip[LBN][LBN][4] = {
  {"!xH","!xH","!xH","!xH","!xH","!xH","!xH","!/H","!xH","!/H","!/H","!xH","!xH","!/H","!xH","!/H","!/H","!/H","!/H","!/S"},
  {"!xH","!xH","!xH","!xH","!xH","!xH","!xH","!/H","!xH","!/H","!/H","!xH","!xH","!/H","!xH","!/H","!/H","!/H","!/H","!/S"},
  {"!xH","!xH","!xH","!xH","!xH","!xH","!xH","!/H","!xH","!/H","!/H","!xH","!xH","!/H","!xH","!/H","!/H","!/H","!/H","!/S"},
  {"!xH","!xH","!xH","!xH","!xH","!xH","!xH","!/H","!xH","!/H","!/H","!xH","!xH","!/H","!xH","!/H","!/H","!/H","!/H","!/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","x/H","x/H","x/H","x/H","x/H","x/H","x/H","x/H","x/H","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxH","xxS"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","xxS","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxH","xxH","xxH","xxH","xxH","xxH","xxH","x/H","xxH","x/H","x/H","xxH","xxH","x/H","xxH","x/H","x/H","x/H","x/H","x/S"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","x/S","x/S","xxS","xxS","x/S","xxS","x/S","x/S","x/S","x/S","x/S"}
};
static const char lb_cm[LBN][LBN][4] = {
  {"!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!/S","!xS","!xS","!xS"},
  {"!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!/S","!xS","!xS","!xS"},
  {"!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!/S","!xS","!xS","!xS"},
  {"!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!xS","!/S","!xS","!xS","!xS"},
  {"/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","//S","/xS","/xS","/xS"},
  {"/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","/xS","//S","/xS","/xS","/xS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS","x/S","xxS","x/S","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS","x/S","xxS","x/S","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS","xxS","xxS","x/S","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS","x/S","xxS","x/S","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","x/S","xxS","xxS","xxS","xxS","xxS","x/S","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","x/S","xxS","xxS","xxS","x/S","xxS","x/S","x/S","xxS","xxS","xxS"},
  {"xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","xxS","x/S","xxS","xxS","xxS"}
};

static int cond_of(char c) { return c == 'x' ? BC_MAYNOT : c == '/' ? BC_CAN : BC_MUST; }
static const char *cname(int c) { static const char *n[] = { "NEUTRAL", "CAN_BREAK", "MAY_NOT_BREAK", "MUST_BREAK" }; return n[c & 3]; }

static ms_sink *lb_text(IDWriteTextAnalyzer *an, const WCHAR *t, UINT32 n, int pat, HRESULT *hr)
{
    ms_src *s = ms_src_new(t, n); ms_sink *k = ms_sink_new(0);
    if (pat) ms_src_cuts(s, ms_patterns[pat], 6);
    *hr = ms_do(an, MA_BREAK, s, 0, n, k);
    ms_src_free(s);
    return k;
}
/* boundary b (1..n-1) between units b-1 and b */
static void check_boundary(const ms_sink *k, UINT32 b, char want, int hard, const char *what, int pat)
{
    int a = BRK_AFTER(k->brk[b - 1]), bf = BRK_BEFORE(k->brk[b]);
    CHECKF(a == bf, "%s boundary %u (pattern %d): after-condition %s but before-condition %s", what, b, pat, cname(a), cname(bf));
    if (hard) CHECKF(a == cond_of(want), "%s boundary %u (pattern %d): got %s want %s", what, b, pat, cname(a), cname(cond_of(want)));
    else if (a != cond_of(want)) t_note("%s boundary %u: got %s, rule-order expectation %s", what, b, cname(a), cname(cond_of(want)));
}
static void row_pairs(int x)
{
    MS_BEGIN(); int y, pi; static const int pats[] = { 0, 1 };
    for (y = 0; y < LBN; y++) for (pi = 0; pi < 2; pi++) {
        WCHAR t[3] = { lb_ch[x], lb_ch[y], 0 }; HRESULT hr; ms_sink *k; char what[32];
        k = lb_text(an, t, 2, pats[pi], &hr);
        CHECK_HR_OK(hr); CHECK(k->n[MK_BREAK] == 1 && ms_sink_exact(k, MK_BREAK, 0, 2));
        snprintf(what, sizeof what, "%s %s", lb_name[x], lb_name[y]);
        check_boundary(k, 1, lb_pair[x][y][0], lb_pair[x][y][1] == 'H', what, pats[pi]);
        CHECK(k->brk[0].padding == 0 && k->brk[1].padding == 0);
        ms_sink_free(k);
    }
    MS_END();
}
static void row_trips(int x)
{
    MS_BEGIN(); int y, pi; static const int pats[] = { 0, 2 };
    for (y = 0; y < LBN; y++) for (pi = 0; pi < 2; pi++) {
        WCHAR t[4] = { lb_ch[x], ' ', lb_ch[y], 0 }; HRESULT hr; ms_sink *k; char what[32]; int hard = lb_trip[x][y][2] == 'H';
        k = lb_text(an, t, 3, pats[pi], &hr);
        CHECK_HR_OK(hr); CHECK(k->n[MK_BREAK] == 1 && ms_sink_exact(k, MK_BREAK, 0, 3));
        snprintf(what, sizeof what, "%s SP %s", lb_name[x], lb_name[y]);
        check_boundary(k, 1, lb_trip[x][y][0], hard, what, pats[pi]);
        check_boundary(k, 2, lb_trip[x][y][1], hard, what, pats[pi]);
        ms_sink_free(k);
    }
    MS_END();
}
static void row_cm(int x)
{
    MS_BEGIN(); int y;
    for (y = 0; y < LBN; y++) {
        WCHAR t[4] = { lb_ch[x], 0x0301, lb_ch[y], 0 }; HRESULT hr; ms_sink *k; char what[32];
        k = lb_text(an, t, 3, 0, &hr);
        CHECK_HR_OK(hr); CHECK(ms_sink_exact(k, MK_BREAK, 0, 3));
        snprintf(what, sizeof what, "%s CM %s", lb_name[x], lb_name[y]);
        check_boundary(k, 1, lb_cm[x][y][0], 0, what, 0);
        check_boundary(k, 2, lb_cm[x][y][1], 0, what, 0);
        ms_sink_free(k);
    }
    MS_END();
}
MT(lbm_pair_first_BK) { row_pairs(0); }
MT(lbm_space_first_BK) { row_trips(0); }
MT(lbm_mark_first_BK) { row_cm(0); }
MT(lbm_pair_first_CR) { row_pairs(1); }
MT(lbm_space_first_CR) { row_trips(1); }
MT(lbm_mark_first_CR) { row_cm(1); }
MT(lbm_pair_first_LF) { row_pairs(2); }
MT(lbm_space_first_LF) { row_trips(2); }
MT(lbm_mark_first_LF) { row_cm(2); }
MT(lbm_pair_first_NL) { row_pairs(3); }
MT(lbm_space_first_NL) { row_trips(3); }
MT(lbm_mark_first_NL) { row_cm(3); }
MT(lbm_pair_first_SP) { row_pairs(4); }
MT(lbm_space_first_SP) { row_trips(4); }
MT(lbm_mark_first_SP) { row_cm(4); }
MT(lbm_pair_first_ZW) { row_pairs(5); }
MT(lbm_space_first_ZW) { row_trips(5); }
MT(lbm_mark_first_ZW) { row_cm(5); }
MT(lbm_pair_first_WJ) { row_pairs(6); }
MT(lbm_space_first_WJ) { row_trips(6); }
MT(lbm_mark_first_WJ) { row_cm(6); }
MT(lbm_pair_first_GL) { row_pairs(7); }
MT(lbm_space_first_GL) { row_trips(7); }
MT(lbm_mark_first_GL) { row_cm(7); }
MT(lbm_pair_first_CL) { row_pairs(8); }
MT(lbm_space_first_CL) { row_trips(8); }
MT(lbm_mark_first_CL) { row_cm(8); }
MT(lbm_pair_first_OP) { row_pairs(9); }
MT(lbm_space_first_OP) { row_trips(9); }
MT(lbm_mark_first_OP) { row_cm(9); }
MT(lbm_pair_first_QU) { row_pairs(10); }
MT(lbm_space_first_QU) { row_trips(10); }
MT(lbm_mark_first_QU) { row_cm(10); }
MT(lbm_pair_first_EX) { row_pairs(11); }
MT(lbm_space_first_EX) { row_trips(11); }
MT(lbm_mark_first_EX) { row_cm(11); }
MT(lbm_pair_first_IS) { row_pairs(12); }
MT(lbm_space_first_IS) { row_trips(12); }
MT(lbm_mark_first_IS) { row_cm(12); }
MT(lbm_pair_first_NU) { row_pairs(13); }
MT(lbm_space_first_NU) { row_trips(13); }
MT(lbm_mark_first_NU) { row_cm(13); }
MT(lbm_pair_first_SY) { row_pairs(14); }
MT(lbm_space_first_SY) { row_trips(14); }
MT(lbm_mark_first_SY) { row_cm(14); }
MT(lbm_pair_first_AL) { row_pairs(15); }
MT(lbm_space_first_AL) { row_trips(15); }
MT(lbm_mark_first_AL) { row_cm(15); }
MT(lbm_pair_first_ID) { row_pairs(16); }
MT(lbm_space_first_ID) { row_trips(16); }
MT(lbm_mark_first_ID) { row_cm(16); }
MT(lbm_pair_first_HY) { row_pairs(17); }
MT(lbm_space_first_HY) { row_trips(17); }
MT(lbm_mark_first_HY) { row_cm(17); }
MT(lbm_pair_first_BA) { row_pairs(18); }
MT(lbm_space_first_BA) { row_trips(18); }
MT(lbm_mark_first_BA) { row_cm(18); }
MT(lbm_pair_first_CM) { row_pairs(19); }
MT(lbm_space_first_CM) { row_trips(19); }
MT(lbm_mark_first_CM) { row_cm(19); }
