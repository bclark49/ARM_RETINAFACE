#include <QApplication>
#include <QPushButton>

int main (int argc, char* argv[]) {
	QApplication app (argc, argv);
	QPushButton button1 ("test");

	button1.show();
	return app.exec();
}
