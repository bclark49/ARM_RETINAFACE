#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>
#include <QImage>
#include <QMessageBox>
//#include <QGraphicsEffect>
//

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
				cv::Mat m (cv::Size (1920, 1080), CV_8UC3, map.data);
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
MainWindow::MainWindow(QWidget* parent, Config* in_conf) : QMainWindow(parent), ui(new Ui::MainWindow) {
	// Set up UI
    ui->setupUi(this);
    setupUI();
	vid_proc_ptr = new Video_Proc (this, in_conf);
	connect (vid_proc_ptr, &Video_Proc::finished, vid_proc_ptr, &Video_Proc::deleteLater);
	connect (vid_proc_ptr, &Video_Proc::finished, vid_proc_ptr, &Video_Proc::quit);
	connect (vid_proc_ptr, &Video_Proc::new_frame_sig, this, &MainWindow::updateFrame);
	connect (ui->startButton, &QPushButton::clicked, this, &MainWindow::toggleVideo);
	connect (ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopVideo);
	connect (this, &MainWindow::stop_vid_proc, vid_proc_ptr, &Video_Proc::set_term_sig);
	vid_proc_ptr->start ();
}

// Free resources
MainWindow::~MainWindow () {
	printf("DESTROY MAIN\n");
	delete vid_proc_ptr;
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
	if (vid_proc_ptr->pipe_running) {
		QMessageBox popup;
		popup.setWindowTitle ("STOP VIDEO!");
		popup.setText ("Please press stop before closing the main window.");
		popup.exec ();
		event->ignore ();
	}
	else {
		vid_proc_ptr->wait ();
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
	vid_proc_ptr->set_toggle_sig (true);
}

void MainWindow::stopVideo () {
	printf("called\n");
    ui->label1->clear();
    ui->label2->clear();
	vid_proc_ptr->set_term_sig (true);
}

void MainWindow::updateFrame (const cv::Mat m) {
	if (!m.empty()) {
		cv::cvtColor(m, m, cv::COLOR_BGR2RGB); 
		QImage frame (m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);
		ui->label1->setPixmap(QPixmap::fromImage(frame));
	}
	else printf("EMPTY\n");
}

Video_Proc::Video_Proc (QObject* parent, Config* in_conf) : QThread(parent) {
	char reader_uri [512];
	pipe_running = false;
	sprintf (reader_uri, "v4l2src device=%s ! video/x-raw, format=%s, width=%d, height=%d, framerate=%d/1 ! tee name=t \
			t. ! queue ! mpph264enc level=42 ! rtph264pay name=pay0 pt=96 ! udpsink name=udpsink host=169.254.73.214 port=9002 sync=false \
			t. ! queue ! videoconvert ! appsink name=appsink sync=false",
			in_conf->dev, in_conf->format, in_conf->res[0], in_conf->res[1], in_conf->fps);
	pipeline = gst_parse_launch (reader_uri, NULL);
	printf ("PARSE LAUNCH\n");
	if (!pipeline) {
		fprintf (stderr, "Failed to create pipeline. Exiting\n");
		return;	
	}
	GstElement* udpsink = gst_bin_get_by_name (GST_BIN (pipeline), "udpsink");
	if (!udpsink) {
		fprintf (stderr, "Failed to retrieve udpsink. Exiting\n");
		return;	
	}
	GstElement* appsink = gst_bin_get_by_name (GST_BIN (pipeline), "appsink");
	if (!appsink) {
		fprintf (stderr, "Failed to retrieve appsink. Exiting\n");
		return;	
	}
	/*
	GstElement *v4l2src, *capsfilter, *tee, *queue_udp, *mpph264enc, *rtph264pay, *udpsink, *queue_videoconvert, *videoconvert, *appsink;
	pipeline = gst_pipeline_new ("Video pipe");
	v4l2src = gst_element_factory_make ("v4l2src", "v4l2src");
	g_object_set (G_OBJECT (v4l2src), "device", in_conf->dev, NULL);
	capsfilter = gst_element_factory_make ("capsfilter", "capsfilter");
	tee = gst_element_factory_make ("tee", "tee");
	// UDP streaming pipe
	queue_udp = gst_element_factory_make ("queue", "queue_udp");
	mpph264enc = gst_element_factory_make ("mpph264enc", "mpph264enc");
	g_object_set (G_OBJECT (mpph264enc), "level", 42, NULL);
	rtph264pay = gst_element_factory_make ("rtph264pay", "rtph264pay");
	g_object_set (G_OBJECT (rtph264pay), "name", "pay0", "pt", 96, NULL);
	udpsink = gst_element_factory_make ("udpsink", "udpsink");
	g_object_set (G_OBJECT (udpsink), "host", "169.254.73.214", "port", 9002, NULL);
	//g_object_set (G_OBJECT (udpsink), "host", "127.0.0.1", "port", 9002, NULL);

	// APPSINK display pipe
	queue_videoconvert = gst_element_factory_make ("queue", "queue_videoconvert");
	videoconvert = gst_element_factory_make ("videoconvert", "videoconvert");
	appsink = gst_element_factory_make ("appsink", "appsink");
	g_object_set (G_OBJECT (appsink), "sync", false, NULL);

	if (!pipeline || !v4l2src || !capsfilter || !tee || !queue_udp || !mpph264enc || !rtph264pay || !udpsink || !queue_videoconvert || !videoconvert || !appsink ) {
		fprintf (stderr, "Failed to create one or more elements. Exiting");
		return;	
	}
	
	char capstr[128];
	sprintf(capstr,"video/x-raw, width=%d, height=%d, framerate=%d/1, format=%s", in_conf->res[0], in_conf->res[1], in_conf->fps, in_conf->format);
	GstCaps *caps = gst_caps_from_string (capstr);
	if(!caps) {
		fprintf (stderr, "Failed to create gst caps from string. Exiting");
		return;	
	}
	g_object_set (G_OBJECT(capsfilter), "caps", caps, NULL);
	gst_bin_add_many (GST_BIN(pipeline), v4l2src, capsfilter, tee, queue_udp, mpph264enc, rtph264pay, udpsink, queue_videoconvert, videoconvert, appsink, NULL);

	// Link elements
	if ( !gst_element_link_many (v4l2src, capsfilter, tee, NULL) ||
		!gst_element_link_many (tee, queue_udp, mpph264enc, rtph264pay, udpsink, NULL) ||
		!gst_element_link_many (tee, queue_videoconvert, videoconvert, appsink, NULL) ) {
		fprintf (stderr, "Failed to link one or more elements. Exiting");
		gst_object_unref (pipeline);
		return;	
	}

	*/
	// Configure appsink callback when frames are received
	cb_dat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
	cb_dat.vid_proc_ptr = this;
	gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &cb_dat.callback, &cb_dat, nullptr);
	loop = g_main_loop_new (nullptr, false);
}

Video_Proc::~Video_Proc () {
	printf("DESTROY\n");
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
	g_main_loop_run (loop);
	pipe_running = false;
	//g_source_remove (to_src);
	g_main_loop_unref (loop);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(pipeline));

	return;
}
