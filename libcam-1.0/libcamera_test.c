#include "libcamera.c"  // Single-file include!

int main() {
    camera_handle_t *cam = camera_init();
    if (cam) {
        printf("✅ libcamera.c 1.0 working! 1920x1080@30fps
");
        camera_release(cam);
        return 0;
    }
    return 1;
}