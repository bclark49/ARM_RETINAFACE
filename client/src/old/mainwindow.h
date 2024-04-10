#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QObject>
#include <QThread>
#include <opencv2/opencv.hpp>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>
#include <thread>

Q_DECLARE_METATYPE(cv::Mat)

GstFlowReturn new_sample_callback (GstAppSink* sink, gpointer data);

struct Config {
	int res[2];
	int fps;
	char dev[16];
	char format[16];
};

class Video_Proc;

// Used to pass data to gstreamer
struct Callback_Data {
	GstAppSinkCallbacks callback;
	Video_Proc* vid_proc_ptr;
};

// Gstreamer wrapper object
class Video_Proc : public QObject {
	Q_OBJECT
public:
	Video_Proc (Config* in_conf);
	//~Video_Proc ();
public slots: 
	void run_pipeline ();
	//GstElement* get_pipeline_ref() {return pipeline;}
	/*void handle_sig () {
		printf("EXTERNAL SIGNALED\n");
		emit vid_ctrl_sig();
   	}*/
signals:
	// Sends start and stop to gstreamer
	// Signal to send images to app
	void new_frame_sig (const cv::Mat frame);
	void finished();
	//void vid_ctrl_sig ();
private:
	GstElement* pipeline;
	GMainLoop* loop;
	Callback_Data cb_dat;
	bool paused;
	//void handle_sig_internal ();
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr, Config* in_conf = nullptr);
    ~MainWindow ();

signals: 
	//void operate ();

protected:
	void keyPressEvent (QKeyEvent *event) override;

private slots:
    void startVideo ();
    void stopVideo ();
    void updateFrame (const cv::Mat frame);

private:
    Ui::MainWindow *ui;
	//GstElement *pipeline;
	Video_Proc* vid_proc_ptr;
	QThread* worker_thread;
    void setupUI ();
	virtual void closeEvent (QCloseEvent *event) override;
	//void gst_pipeline_thread ();
	//std::thread* gst_pipeline_thread_ptr;
};

#endif // MAINWINDOW_H
