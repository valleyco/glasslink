#include "bind.h"
#include "fake_display.h"
#include "test_assert.h"

#include <string.h>

static void test_define_and_set(void)
{
    const bind_slot_t *s;
    fake_display_reset();
    bind_reset();

    ASSERT_EQ_INT(BIND_OK, bind_define(0, 10, 20, 0xFFFF, 0x0010, 1, 8,
                                       (const uint8_t *)"hi", 2));
    s = bind_get(0);
    ASSERT_TRUE(s && s->used);
    ASSERT_EQ_INT(0, strcmp(s->text, "hi"));
    /* 'h'/'i' light some fg pixels in the slot box */
    {
        int lit = 0;
        for (int y = 20; y < 27; y++) {
            for (int x = 10; x < 10 + 8 * 6; x++) {
                if (fake_display_get_pixel(x, y) == 0xFFFF) {
                    lit++;
                }
            }
        }
        ASSERT_TRUE(lit > 3);
    }

    ASSERT_EQ_INT(BIND_OK, bind_set_text(0, (const uint8_t *)"OK", 2));
    ASSERT_EQ_INT(0, strcmp(bind_get(0)->text, "OK"));
}

static void test_bad_slot(void)
{
    bind_reset();
    ASSERT_EQ_INT(BIND_ERR_SLOT, bind_define(99, 0, 0, 0, 0, 1, 4, NULL, 0));
    ASSERT_EQ_INT(BIND_ERR_SLOT, bind_set_text(0, (const uint8_t *)"x", 1));
}

int main(void)
{
    test_define_and_set();
    test_bad_slot();
    return test_report();
}
