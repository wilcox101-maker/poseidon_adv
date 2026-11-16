/*
 * lab_mode — guided educational exercises screen (PUIE #6).
 *
 * A read-mostly "lesson" panel: objective list with check-offs, a progress
 * bar, and lesson navigation. Steps auto-check when the firmware reports a
 * matching event via labmode_notify() (the PSPE event-seam); the user can
 * also toggle them by hand to explore.
 *
 * Content is PLACEHOLDER (see labmode.cpp) pending the real curriculum.
 *
 * Invariants: drawn only from this loopTask body (I1); no per-frame heap
 * (I2); plain function, reached through menu_execute_action (I4/I6).
 */
#include "../ui.h"
#include "../input.h"
#include "../labmode.h"

static void draw(int sel)
{
    auto &d = M5Cardputer.Display;
    ui_header("LAB MODE", labmode_lesson_title(labmode_current()));

    const int total = labmode_step_count();
    const int done  = labmode_steps_done();

    /* Progress line + bar, just under the header rule. */
    int py = BODY_Y + 16;
    d.setTextColor(T_DIM, T_BG);
    d.setCursor(4, py);
    d.printf("%d/%d done", done, total);

    const int bx = 70, bw = SCR_W - 70 - 6, bh = 6;
    d.drawRect(bx, py, bw, bh, T_DIM);
    if (total > 0) {
        int fill = (bw - 2) * done / total;
        if (fill > 0) d.fillRect(bx + 1, py + 1, fill, bh - 2, T_GOOD);
    }

    /* Step list. */
    int ly = py + 12;
    const int rh = 11;
    for (int i = 0; i < total; ++i) {
        bool cur = (i == sel);
        uint16_t bg = cur ? T_SEL_BG : T_BG;
        uint16_t fg = labmode_step_done(i) ? T_GOOD : T_FG;
        d.fillRect(0, ly - 1, SCR_W, rh, bg);
        d.setTextColor(T_DIM, bg);
        d.setCursor(4, ly);
        d.print(labmode_step_done(i) ? "[x]" : "[ ]");
        d.setTextColor(fg, bg);
        d.setCursor(28, ly);
        d.print(labmode_step_text(i));
        ly += rh;
    }
}

void feat_lab_mode(void)
{
    auto &d = M5Cardputer.Display;
    labmode_select(labmode_current());   /* ensure a lesson is active */

    ui_clear_body();
    ui_draw_footer(";/. step  SPACE mark  n=lesson  `=back");

    int sel = 0;
    bool dirty = true;
    while (true) {
        if (dirty) {
            d.fillRect(0, BODY_Y, SCR_W, BODY_H, T_BG);
            draw(sel);
            dirty = false;
        }

        uint16_t k = input_poll();
        if (k == PK_NONE) { delay(20); continue; }

        const int total = labmode_step_count();
        if (k == PK_ESC) return;
        else if (k == ';' || k == PK_UP)   { if (sel > 0) { sel--; dirty = true; } }
        else if (k == '.' || k == PK_DOWN) { if (sel < total - 1) { sel++; dirty = true; } }
        else if (k == PK_SPACE || k == PK_ENTER) {
            labmode_mark(sel, !labmode_step_done(sel));
            dirty = true;
        } else if (k == 'n' || k == 'N') {
            int next = (labmode_current() + 1) % labmode_lesson_count();
            labmode_select(next);
            sel = 0;
            dirty = true;
        }
    }
}
