#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPixmap>
#include <QImage>
//#include <QGraphicsEffect>

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
				printf("CALLBACK\n");
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

// Constructor: inherits QWidget
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	// Set up UI
    ui->setupUi(this);
    setupUI();
	vid_proc.moveToThread (&worker_thread);

	// Signals connected to slots
	//connect (&worker_thread, &QThread::finished, vid_proc, &QObject::deleteLater);
	connect (&worker_thread, &QThread::started, &vid_proc, &Video_Proc::run_pipeline);
	//connect (&vid_proc, &Video_Proc::new_frame_sig, this, &MainWindow::updateFrame);
	//connect (ui->startButton, &QPushButton::clicked, vid_proc, &Video_Proc::handle_sig);
	//connect (ui->stopButton, &QPushButton::clicked, vid_proc, &Video_Proc::handle_sig);
	//connect (ui->startButton, &QPushButton::clicked, this, &MainWindow::startVideo);
	//connect (ui->stopButton, &QPushButton::clicked, this, &MainWindow::stopVideo);

	// gstreamer wrapper object/thread created
	//Video_Proc *vid_proc = new Video_Proc;
	//pipeline = vid_proc->get_pipeline_ref();
	worker_thread.start ();
	printf("THREAD START\n");
	//vid_proc.run_pipeline();
	//emit operate ();
}

MainWindow::~MainWindow () {
	//this->gst_pipeline_thread_ptr->join();
    delete ui;
}

void MainWindow::setupUI () {
    setWindowState(Qt::WindowFullScreen);
	ui->label1->setFixedSize(900, 1020);
	//ui->label2->setFixedSize(940, 700);
}

void MainWindow::closeEvent (QCloseEvent *event) {
	event->accept();
	//closeEvent (event);
}

void MainWindow::keyPressEvent (QKeyEvent *event) {
	if (event->key() == Qt::Key_Escape) {
		close();
	}
}

void MainWindow::startVideo () {
	//kill (getpid(), SIGUSR1);
	printf("CLICKED\n");
	/*
	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting");
		//gst_object_unref (pipeline);
	}
	else printf("GST PLAYING\n");
	*/

}

void MainWindow::stopVideo () {
    ui->label1->clear();
    ui->label2->clear();
	printf("CLICKED2\n");
	//kill (getpid(), SIGUSR1);
	/*
	if (gst_element_set_state (pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to paused. Exiting");
		//gst_object_unref (pipeline);
	}
	else printf("GST PAUSED\n");
	*/
}

void MainWindow::updateFrame (const cv::Mat m) {
	printf("UPDATE\n");
	if (!m.empty()) {
		printf("MAT %dx%d\n", m.cols, m.rows);
		cv::cvtColor(m, m, cv::COLOR_BGR2RGB); 
		QImage frame (m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);
		ui->label1->setPixmap(QPixmap::fromImage(frame));
	}
	else printf("EMPTY\n");
}

Video_Proc::Video_Proc () {
	GstElement *v4l2src, *capsfilter, *tee, *queue_udp, *mpph264enc, *rtph264pay, *udpsink, *queue_videoconvert, *videoconvert, *appsink;
	pipeline = gst_pipeline_new ("Video pipe");
	v4l2src = gst_element_factory_make ("v4l2src", "v4l2src");
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

	// APPSINK display pipe
	queue_videoconvert = gst_element_factory_make ("queue", "queue_videoconvert");
	videoconvert = gst_element_factory_make ("videoconvert", "videoconvert");
	appsink = gst_element_factory_make ("appsink", "appsink");
	//g_object_set (G_OBJECT (appsink), "sync", false, NULL);

	if (!pipeline || !v4l2src || !capsfilter || !tee || !queue_udp || !mpph264enc || !rtph264pay || !udpsink || !queue_videoconvert || !videoconvert || !appsink ) {
		fprintf (stderr, "Failed to create one or more elements. Exiting");
		return;	
	}
	
	GstCaps *caps = gst_caps_from_string ("video/x-raw, width=1920, height=1080, framerate=60/1, format=BGR");
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

	// Configure appsink callback when frames are received
	Callback_Data cb_dat;
	cb_dat.callback = {nullptr, nullptr, new_sample_callback, nullptr};
	cb_dat.vid_proc_ptr = this;
	gst_app_sink_set_callbacks (GST_APP_SINK (appsink), &cb_dat.callback, &cb_dat, nullptr);
	//connect (this, &Video_Proc::vid_ctrl_sig, this, &Video_Proc::handle_sig_internal);
	paused = false;
	loop = g_main_loop_new (nullptr, false);
}

/*
void Video_Proc::handle_sig_internal () {
	printf("INTERNAL SIGNALED\n");
    //ui->label1->clear();
    //ui->label2->clear();
	if (!paused) {
		if (gst_element_set_state (pipeline, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE){
			fprintf (stderr, "Failed to set state to paused. Exiting");
			gst_object_unref (pipeline);
		}
		else printf("GST PAUSED\n");
		paused = true;
	}
	else {
		if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
			fprintf (stderr, "Failed to set state to playing. Exiting");
			gst_object_unref (pipeline);
		}
		else printf("GST PLAYING\n");
		paused = false;
	}
}*/

// Seperate class for gstreamer. only runs in this thread
void Video_Proc::run_pipeline () {
	if (pipeline == NULL) printf("NULL3\n");
	//GMainLoop *loop = g_main_loop_new (nullptr, false);
	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting");
		gst_object_unref (pipeline);
	}
	else printf("GST PLAYING\n");
	g_main_loop_run (loop);
	printf("LOOP DONE\n");
	g_main_loop_unref (loop);
	gst_element_set_state (pipeline, GST_STATE_NULL);
	gst_object_unref (GST_OBJECT(pipeline));
	return;
}
/*
void Video_Proc::startVideo()
{
	if (gst_element_set_state (pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting");
		gst_object_unref (pipeline);
		return;	
	}
	else printf("GST PLAYING\n");
}

void Video_Proc::stopVideo()
{
    ui->label1->clear();
    //ui->label2->clear();
	if (gst_element_set_state (pipeline, GST_STATE_PAUSE) == GST_STATE_CHANGE_FAILURE){
		fprintf (stderr, "Failed to set state to playing. Exiting");
		gst_object_unref (pipeline);
		return;	
	}
	else printf("GST PLAYING\n");
}
*/
