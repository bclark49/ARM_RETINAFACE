#include "my_gst.h"
#include <unistd.h>

int main (int argc, char* argv[]) {
	if (!gst_debug_is_active () ) {
		gst_debug_set_active (true);
		GstDebugLevel dgblevel = gst_debug_get_default_threshold ();
		if (dgblevel < GST_LEVEL_ERROR) {
			dgblevel = GST_LEVEL_ERROR;
			gst_debug_set_default_threshold (dgblevel);
		}
	}
	XInitThreads();
	gst_init(&argc, &argv);

	Video_Proc vid_proc;

	while (!vid_proc.is_finished ()) {
		printf("WAITING\n");
		sleep (1);
	}
	return 0;
}
