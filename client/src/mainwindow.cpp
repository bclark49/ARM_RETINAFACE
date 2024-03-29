#include "mainwindow.h"
#include "ui_mainwindow.h"

extern const int WIDTH;
extern const int HEIGHT;

void annotate_frame (QImage &m, std::chrono::steady_clock::time_point Tbegin, 
			std::chrono::steady_clock::time_point Tend, 
			int &Fcnt, float (&FPS)[16], char (&str)[64] ) {
	float f;
	int i, point, font;
	point = 200 - (WIDTH * 100 / 1920);
	font = 24 - (WIDTH * 12 / 1920);
	f = std::chrono::duration_cast <std::chrono::milliseconds> (Tend-Tbegin).count ();
	if (f>0.0) FPS[((Fcnt++)&0x0F)] = 1000.0 / f;
	for (f=0.0, i=0; i<16; i++) {f+=FPS[i];}
	sprintf (str, "FPS: %0.2f, Resolution: %d/%d", f/16, m.width (), m.height ());
	QPainter painter (&m);
	QPen pen (Qt::black, 32, Qt::SolidLine);
	painter.setPen (pen);
	painter.setFont (QFont ("Times", font, QFont::Bold));
	painter.drawText (QPoint (point,point), str);
}

// This function is called when a sample is available
// sink: appsink from gstreamer pipe
// data: holds object for emitting frame to app
// return: error for gstreamer
GstFlowReturn new_sample_callback (GstAppSink* sink, gpointer data) {
	GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK(sink));
	Callback_Data* cb_dat = static_cast<Callback_Data*>(data);
	if (sample != nullptr) {
		GstBuffer* buffer = gst_sample_get_buffer (sample);
		if (buffer != nullptr) {
			GstMapInfo map;
			if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
				cv::Mat m (cv::Size (WIDTH, HEIGHT), CV_8UC3, map.data);
				if (!cb_dat->vid_proc_ptr->udpsrc) {
					printf ("Camsrc SIZE: %d\n", m.total () * m.elemSize ());
					printf ("Camsrc RES: %d/%d\n", m.cols, m.rows);
				}
				else
					printf ("udpsrc SIZE: %d\n", m.total () * m.elemSize ());
				// the entire matrix is sent to stop dangling pointer
				emit cb_dat->vid_proc_ptr->new_frame_sig (m.clone());
				gst_buffer_unmap (buffer, &map);
				gst_sample_unref (sample);
				return GST_FLOW_OK;
			}
			gst_buffer_unmap (buffer, &map);
		}
		gst_sample_unref (sample);
	}
	printf("GST_ERROR\n");
	return GST_FLOW_ERROR;
}

// Constructor: inherits QMainWindow
MainWindow::MainWindow(QWidget* parent, char* udp_src_str, char* cam_src_str ) : QMainWindow(parent), ui(new Ui::MainWindow) {
	// Set up UI
    ui->setupUi(this);
    setupUI();
	udp_src_ptr = new Video_Proc (this, udp_src_str, true);
	cam_src_ptr = new Video_Proc (this, cam_src_str, false);
	connect (udp_src_ptr, &Video_Proc::finished, udp_src_ptr, &Video_Proc::deleteLater);
	connect (cam_src_ptr, &Video_Proc::finished, cam_src_ptr, &Video_Proc::deleteLater);
	connect (udp_src_ptr, &Video_Proc::finished, udp_src_ptr, &Video_Proc::quit);
	connect (cam_src_ptr, &Video_Proc::finished, cam_src_ptr, &Video_Proc::quit);
	connect (udp_src_ptr, &Video_Proc::new_frame_sig, this, &MainWindow::updateFrameAfter);
	connect (cam_src_ptr, &Video_Proc::new_frame_sig, this, &MainWindow::updateFrameBefore);
	connect (ui->startButton, &QPushButton::clicked, this, &MainWindow::toggleVideo);
	connect (ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopVideo);
	connect (this, &MainWindow::stop_vid_proc, cam_src_ptr, &Video_Proc::set_term_sig);
	connect (this, &MainWindow::stop_vid_proc, udp_src_ptr, &Video_Proc::set_term_sig);

    for (int i=0; i<16; i++) udpsrc_FPS[i] = 0.0;
	udpsrc_Fcnt = 0;
	Tudpsrc_prev_end = std::chrono::steady_clock::now ();
    for (int i=0; i<16; i++) camsrc_FPS[i] = 0.0;
	camsrc_Fcnt = 0;
	Tcamsrc_prev_end = std::chrono::steady_clock::now ();

	udp_src_ptr->start ();
	cam_src_ptr->start ();
}

// Free resources
MainWindow::~MainWindow () {
	printf("DESTROY MAINWINDOW\n");
	delete cam_src_ptr;
	delete udp_src_ptr;
    delete ui;
}

void MainWindow::setupUI () {
    //setWindowState(Qt::WindowFullScreen);
	//ui->label1->setFixedSize(900, 1020);
	//ui->label2->setFixedSize(940, 700);
}

// Called when window is closed. Used to syncronize exit
void MainWindow::closeEvent (QCloseEvent *event) {
	//vid_proc_ptr->terminate ();
	if (cam_src_ptr->pipe_running || udp_src_ptr->pipe_running) {
		QMessageBox popup;
		popup.setWindowTitle ("STOP VIDEO!");
		popup.setText ("Please press stop before closing the main window.");
		popup.exec ();
		event->ignore ();
	}
	else {
		cam_src_ptr->wait ();
		udp_src_ptr->wait ();
		event->accept ();
	}
}

// Called when esc key pressed
void MainWindow::keyPressEvent (QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape) {
		close();
	}
}

void MainWindow::toggleVideo () {
	cam_src_ptr->set_toggle_sig (true);
	udp_src_ptr->set_toggle_sig (true);
}

void MainWindow::stopVideo () {
    ui->label1->clear();
    ui->label2->clear();
	cam_src_ptr->set_term_sig (true);
	udp_src_ptr->set_term_sig (true);
}

void MainWindow::updateFrameAfter (cv::Mat m) {
	char str [64];
	std::chrono::steady_clock::time_point Tbegin, Tend;
	Tbegin = std::chrono::steady_clock::now ();
	if (!m.empty()) {
		cv::cvtColor(m, m, cv::COLOR_BGR2RGB); 
		QImage frame (m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);
		if (frame.isNull ()) printf ("NULL\n");

		annotate_frame (frame, Tudpsrc_prev_end, Tbegin, udpsrc_Fcnt, udpsrc_FPS, str);

		ui->label2->setPixmap(QPixmap::fromImage(frame));

		Tudpsrc_prev_end = std::chrono::steady_clock::now ();
	}
	else printf("EMPTY\n");
}

void MainWindow::updateFrameBefore (cv::Mat m) {
	char str [64];
	std::chrono::steady_clock::time_point Tbegin;
	Tbegin = std::chrono::steady_clock::now ();
	if (!m.empty()) {
		cv::cvtColor(m, m, cv::COLOR_BGR2RGB); 
		QImage frame (m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);
		if (frame.isNull ()) printf ("NULL\n");

		annotate_frame (frame, Tcamsrc_prev_end, Tbegin, camsrc_Fcnt, camsrc_FPS, str);

		ui->label1->setPixmap(QPixmap::fromImage(frame));

		Tcamsrc_prev_end = std::chrono::steady_clock::now ();
	}
	else printf("EMPTY\n");
}

Video_Proc::Video_Proc (QObject* parent, char* pipe_str, bool udpsrc) : QThread(parent) {
	pipe_running = false;
	this->udpsrc = udpsrc;
	pipeline = gst_parse_launch (pipe_str, NULL);
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline. Exiting\n");
		return;	
	}
	GstElement* udpsink;
	if (!udpsrc) {
		udpsink = gst_bin_get_by_name (GST_BIN (pipeline), "udpsink");
		if (!udpsink) {
			fprintf (stderr, "Failed to retrieve udpsink. Exiting\n");
			return;	
		}
	}

	GstElement* appsink = gst_bin_get_by_name (GST_BIN (pipeline), "appsink");
	if (!appsink) {
		fprintf (stderr, "Failed to retrieve appsink. Exiting\n");
		return;	
	}

	// Configure appsink callback when frames are received
	if (udpsrc) cb_dat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
	else cb_dat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
	cb_dat.vid_proc_ptr = this;
	gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &cb_dat.callback, &cb_dat, nullptr);
	loop = g_main_loop_new (nullptr, false);
}

Video_Proc::~Video_Proc () {
	printf("DESTROY VID_PROC\n");
	// Cleanup
	QThread::msleep (100);
	emit finished();
}

gboolean Video_Proc::int_src_cb (gpointer data) {
	Video_Proc* vid_proc_ptr = static_cast<Video_Proc*>(data);
	//QMutexLocker locker (&vid_proc_ptr->mutex);
	if (vid_proc_ptr->term_sig == true) {
		g_main_loop_quit (vid_proc_ptr->loop);
		return G_SOURCE_REMOVE;
	}
	else if (vid_proc_ptr->toggle_sig == true) {
		if (vid_proc_ptr->paused == true) {
			if (gst_element_set_state (vid_proc_ptr->pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
				fprintf (stderr, "Failed to set state to playing");
				g_main_loop_quit (vid_proc_ptr->loop);
				return G_SOURCE_REMOVE;
			}
			vid_proc_ptr->paused = false;
		}
		else {
			if (gst_element_set_state (vid_proc_ptr->pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE){
				fprintf (stderr, "Failed to set state to paused");
				g_main_loop_quit (vid_proc_ptr->loop);
				return G_SOURCE_REMOVE;
			}
			vid_proc_ptr->paused = true;
		}
		vid_proc_ptr->set_toggle_sig (false);
	}
	return G_SOURCE_CONTINUE;
}

// Seperate class for gstreamer. only runs in this thread
void Video_Proc::run () {
	g_timeout_add (100, reinterpret_cast<GSourceFunc>(&Video_Proc::int_src_cb), this);

	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting");
		gst_object_unref (pipeline);
	}
	term_sig = false;
	toggle_sig = false;
	paused = false;
	pipe_running = true;
	printf ("MAIN LOOP RUNNING\n");
	g_main_loop_run (loop);
	printf ("MAIN LOOP DONE\n");
	pipe_running = false;
	//g_source_remove (to_src);
	g_main_loop_unref (loop);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(pipeline));

	return;
}
