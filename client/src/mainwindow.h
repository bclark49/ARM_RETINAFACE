#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QKeyEvent>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QMessageBox>

#include <opencv2/opencv.hpp>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <glib.h>

#include <thread>

Q_DECLARE_METATYPE(cv::Mat)

struct Config {
	int res[2];
	int fps;
	char dev[16];
	char format[16];
	char server_ip [16];
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
	bool pipe_running;
	bool udpsrc;
	Video_Proc (QObject* parent=nullptr, char* pipe_str=nullptr, bool udpsink=0);
	~Video_Proc ();
public slots:
	void set_term_sig (bool var) {
		//printf("SETSIG\n");
		term_sig = var;
	}
	void set_toggle_sig (bool var) {
		//printf("SETSIG\n");
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
    MainWindow(QWidget *parent = nullptr, char* udp_src_str = nullptr, char* cam_src_str = nullptr);
    ~MainWindow ();
protected:
	void keyPressEvent (QKeyEvent *event) override;
signals:
	void stop_vid_proc (bool var);
private slots:
    void toggleVideo ();
    void stopVideo ();
    void updateFrameBefore (cv::Mat frame);
    void updateFrameAfter (cv::Mat frame);
private:
    Ui::MainWindow *ui;
	QThread* worker_thread;
	Video_Proc* udp_src_ptr;
	QPainter label2_painter;
	float udpsrc_FPS [16];
	int udpsrc_Fcnt;
	std::chrono::steady_clock::time_point Tudpsrc_prev_end;
	Video_Proc* cam_src_ptr;
	QPainter label1_painter;
	float camsrc_FPS [16];
	int camsrc_Fcnt;
	std::chrono::steady_clock::time_point Tcamsrc_prev_end;
    void setupUI ();
	virtual void closeEvent (QCloseEvent *event) override;
};

#endif // MAINWINDOW_H
