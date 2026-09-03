#ifndef CAMERA_H_HEADER
#define CAMERA_H_HEADER

#define CAM_WALK_SPEED 1.4f
#define CAM_TURN_RATE 2.5f
#define CAM_FOV_DEG 85.0f
#define CAM_BOB_PX 3
#define CAM_STEP_HZ 1.6f

typedef struct {
    float x, y, angle;             /* tiles, radians; angle 0 faces +x, +pi/2 faces +y */
    float target_x, target_y;
    int   walking, auto_face;
    float target_angle;
    int   turning;
    float bob_phase;
    int   pitch_px;                /* horizon offset driven by the hunt (dip, hop) */
    float speed;
} Camera;

void  camera_init(Camera *c, float x, float y, float angle);
void  camera_walk_to(Camera *c, float x, float y);
void  camera_stop(Camera *c);
void  camera_face(Camera *c, float x, float y);
void  camera_turn_to(Camera *c, float angle);
int   camera_arrived(const Camera *c);
int   camera_facing(const Camera *c, float tol);
float camera_dist(const Camera *c, float x, float y);
void  camera_step(Camera *c, float dt);
int   camera_bob_px(const Camera *c);
float camera_fov(void);
float camera_proj(int w);      /* (w/2) / tan(fov/2) */

#endif
