#include "fake_display.h"
#include "hal_display.h"
#include "render.h"
#include "test_assert.h"

#include <string.h>

static void test_clear_black(void)
{
    fake_display_reset();
    render_clear(0x0000);
    ASSERT_EQ_INT((int)fake_display_panel_len(),
                  (int)fake_display_count_color(0x0000));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(0, 0));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(319, 239));
}

static void test_clear_color(void)
{
    const uint16_t red = 0xF800;
    fake_display_reset();
    render_clear(red);
    ASSERT_EQ_INT((int)fake_display_panel_len(),
                  (int)fake_display_count_color(red));
    ASSERT_EQ_U16(red, fake_display_get_pixel(100, 100));
}

static void test_fill_rect_center(void)
{
    const uint16_t blue = 0x001F;
    fake_display_reset();
    render_clear(0x0000);
    ASSERT_EQ_INT(0, render_fill_rect(10, 20, 30, 40, blue));
    ASSERT_EQ_U16(blue, fake_display_get_pixel(10, 20));
    ASSERT_EQ_U16(blue, fake_display_get_pixel(39, 59));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(9, 20));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(10, 19));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(40, 20));
    ASSERT_EQ_INT(30 * 40, (int)fake_display_count_color(blue));
}

static void test_fill_rect_clip_edges(void)
{
    const uint16_t green = 0x07E0;
    fake_display_reset();
    render_clear(0x0000);
    /* overhangs left/top and right/bottom */
    ASSERT_EQ_INT(0, render_fill_rect(-5, -5, 15, 15, green));
    ASSERT_EQ_U16(green, fake_display_get_pixel(0, 0));
    ASSERT_EQ_U16(green, fake_display_get_pixel(9, 9));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(10, 10));
    ASSERT_EQ_INT(10 * 10, (int)fake_display_count_color(green));

    fake_display_reset();
    render_clear(0x0000);
    ASSERT_EQ_INT(0, render_fill_rect(310, 230, 20, 20, green));
    ASSERT_EQ_U16(green, fake_display_get_pixel(319, 239));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(309, 230));
    ASSERT_EQ_INT(10 * 10, (int)fake_display_count_color(green));
}

static void test_blit_pattern(void)
{
    uint16_t src[4 * 3];
    for (int i = 0; i < 4 * 3; i++) {
        src[i] = (uint16_t)(0x1000 + i);
    }
    fake_display_reset();
    render_clear(0x0000);
    ASSERT_EQ_INT(0, render_blit_rect(2, 3, 4, 3, src));
    ASSERT_EQ_U16(0x1000, fake_display_get_pixel(2, 3));
    ASSERT_EQ_U16(0x1003, fake_display_get_pixel(5, 3));
    ASSERT_EQ_U16(0x1004, fake_display_get_pixel(2, 4));
    ASSERT_EQ_U16(0x100B, fake_display_get_pixel(5, 5));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(1, 3));
}

static void test_blit_clip_negative_origin(void)
{
    uint16_t src[4 * 4];
    for (int i = 0; i < 16; i++) {
        src[i] = (uint16_t)(0xA000 + i);
    }
    fake_display_reset();
    render_clear(0x0000);
    /* src (0,0) maps to panel (-1,-1); visible from src (1,1) */
    ASSERT_EQ_INT(0, render_blit_rect(-1, -1, 4, 4, src));
    /* Visible 3×3 from src (1,1)..(3,3) → panel (0,0)..(2,2) */
    ASSERT_EQ_U16(0xA005, fake_display_get_pixel(0, 0)); /* src 1*4+1 */
    ASSERT_EQ_U16(0xA00F, fake_display_get_pixel(2, 2)); /* src 3*4+3 */
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(3, 3));
    ASSERT_EQ_INT(9, (int)(fake_display_panel_len() -
                           fake_display_count_color(0x0000)));
}

static void test_blit_rejects_bad_args(void)
{
    uint16_t px = 0xFFFF;
    fake_display_reset();
    ASSERT_EQ_INT(-1, render_blit_rect(0, 0, 0, 1, &px));
    ASSERT_EQ_INT(-1, render_blit_rect(0, 0, 1, 0, &px));
    ASSERT_EQ_INT(-1, render_blit_rect(0, 0, 1, 1, NULL));
    ASSERT_EQ_INT(-1, render_fill_rect(0, 0, -1, 1, 0));
}

static void test_fill_poly_triangle(void)
{
    const uint16_t yel = 0xFFE0;
    const int16_t pts[] = {40, 10, 10, 50, 70, 50};
    fake_display_reset();
    render_clear(0x0000);
    ASSERT_EQ_INT(0, render_fill_poly(pts, 3, yel));
    ASSERT_EQ_U16(yel, fake_display_get_pixel(40, 20));
    ASSERT_EQ_U16(0x0000, fake_display_get_pixel(5, 5));
    ASSERT_TRUE(fake_display_count_color(yel) > 100);
}

static void test_draw_line_horizontal(void)
{
    const uint16_t c = 0xF800;
    fake_display_reset();
    render_clear(0x0000);
    ASSERT_EQ_INT(0, render_draw_line(10, 15, 25, 15, c));
    ASSERT_EQ_U16(c, fake_display_get_pixel(10, 15));
    ASSERT_EQ_U16(c, fake_display_get_pixel(25, 15));
    ASSERT_EQ_INT(16, (int)fake_display_count_color(c));
}

static void test_draw_cubic_bezier_endpoints(void)
{
    const uint16_t c = 0x07E0;
    fake_display_reset();
    render_clear(0x0000);
    ASSERT_EQ_INT(0, render_draw_cubic_bezier(0, 0, 10, 0, 20, 0, 30, 0, c));
    ASSERT_EQ_U16(c, fake_display_get_pixel(0, 0));
    ASSERT_EQ_U16(c, fake_display_get_pixel(30, 0));
}

static void test_hal_size(void)
{
    int w = -1;
    int h = -1;
    hal_display_init();
    hal_display_get_size(&w, &h);
    ASSERT_EQ_INT(HAL_DISPLAY_WIDTH, w);
    ASSERT_EQ_INT(HAL_DISPLAY_HEIGHT, h);
}

int main(void)
{
    test_hal_size();
    test_clear_black();
    test_clear_color();
    test_fill_rect_center();
    test_fill_rect_clip_edges();
    test_blit_pattern();
    test_blit_clip_negative_origin();
    test_blit_rejects_bad_args();
    test_fill_poly_triangle();
    test_draw_line_horizontal();
    test_draw_cubic_bezier_endpoints();
    return test_report();
}
