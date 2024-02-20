#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QObject>
#include <QThread>
#include <QMutex>
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
class Video_Proc : public QThread {
	Q_OBJECT
public:
	Video_Proc (QObject* parent=nullptr, Config* in_conf=nullptr);
	~Video_Proc ();
public slots:
	void set_term_sig (bool var) {
		printf("SETSIG\n");
		//QMutexLocker locker(&mutex);
		term_sig = var;
	}
	void set_toggle_sig (bool var) {
		printf("SETSIG\n");
		//QMutexLocker locker(&mutex);
		toggle_sig = var;
	}
signals:
	// Signal to send images to app
	void new_frame_sig (const cv::Mat frame);
	void finished();
private:
	GstElement* pipeline;
	GMainLoop* loop;
	Callback_Data cb_dat;
	bool term_sig;
	bool toggle_sig;
	bool paused;
	QMutex mutex;
	static gboolean int_src_cb (gpointer data);
protected:
	void run () override;
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
protected:
	void keyPressEvent (QKeyEvent *event) override;
signals:
	void stop_vid_proc (bool var);
private slots:
    void toggleVideo ();
    void stopVideo ();
    void updateFrame (const cv::Mat frame);
private:
    Ui::MainWindow *ui;
	Video_Proc* vid_proc_ptr;
	QThread* worker_thread;
    void setupUI ();
	virtual void closeEvent (QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
