#include "test.h"
#include "camera.h"
#include "trig.h"

#define DT (1.0f / 60.0f)

void test_camera(void) {
    Camera c;
    camera_init(&c, 2.5f, 2.5f, 0.0f);
    CHECK(camera_arrived(&c));
    CHECK_EQ(camera_bob_px(&c), 0);
    CHECK_NEAR(camera_fov(), 85.0f * TRIG_PI / 180.0f, 1e-5);
    CHECK_NEAR(camera_proj(320), 160.0f / 0.91633f, 0.5);
    CHECK_NEAR(camera_proj(640), 2.0f * camera_proj(320), 0.01);
    CHECK_NEAR(camera_dist(&c, 5.5f, 6.5f), 5.0f, 1e-3);

    /* walk 1.4 tiles along +x in one second, auto-facing +x */
    camera_walk_to(&c, 3.9f, 2.5f);
    CHECK(!camera_arrived(&c));
    int bob_seen = 0;
    for (int i = 0; i < 59; i++) { camera_step(&c, DT); int b = camera_bob_px(&c); CHECK(b >= -3 && b <= 3); if (b != 0) bob_seen = 1; }
    CHECK(!camera_arrived(&c));
    CHECK(bob_seen);
    for (int i = 0; i < 3; i++) camera_step(&c, DT);
    CHECK(camera_arrived(&c));
    CHECK_NEAR(c.x, 3.9f, 0.01); CHECK_NEAR(c.y, 2.5f, 1e-4);
    CHECK_EQ(camera_bob_px(&c), 0);

    /* walking toward +y turns the view to +pi/2 at 2.5 rad/s: about 0.63 s */
    camera_walk_to(&c, 3.9f, 5.0f);
    for (int i = 0; i < 30; i++) camera_step(&c, DT);       /* 0.5 s: not yet facing */
    CHECK(!camera_facing(&c, 0.05f));
    for (int i = 0; i < 12; i++) camera_step(&c, DT);       /* 0.7 s total */
    CHECK(camera_facing(&c, 0.05f));
    CHECK_NEAR(c.angle, TRIG_HALF_PI, 0.05);
    camera_stop(&c);
    CHECK(camera_arrived(&c));

    /* turning takes the short way around the seam */
    camera_init(&c, 0, 0, 3.0f);
    camera_turn_to(&c, -3.0f);
    camera_step(&c, DT);
    CHECK(c.angle > 3.0f || c.angle < -3.0f);               /* moved toward +pi, not back through 0 */
    for (int i = 0; i < 12; i++) camera_step(&c, DT);
    CHECK(camera_facing(&c, 0.01f));
    CHECK_NEAR(c.angle, -3.0f, 0.01);

    /* face a point: due -y is -pi/2 */
    camera_init(&c, 1, 1, 0);
    camera_face(&c, 1.0f, -4.0f);
    CHECK_NEAR(c.target_angle, -TRIG_HALF_PI, 0.01);
    CHECK_EQ(c.auto_face, 0);
    for (int i = 0; i < 60; i++) camera_step(&c, DT);
    CHECK(camera_facing(&c, 0.01f));

    /* pitch is plain state the hunt drives */
    c.pitch_px = 12; CHECK_EQ(c.pitch_px, 12);
}
