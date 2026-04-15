#include <QApplication>
#include "startwindow.h"
#include "spihandler.h"

int main(int argc, char *argv[]) 
{
    QApplication a(argc, argv);

    // Initialize SPI system once
    SpiHandler *spi = SpiHandler::instance();
    if (spi->init()) {
        std::cout << "SPI initialized successfully at startup." << std::endl;
    } else {
        std::cerr << "Failed to initialize SPI!" << std::endl;
    }

    StartWindow s;
    s.showFullScreen();

    int ret = a.exec();

    gpioTerminate();  // terminate pigpio once on exit
    return ret;
}
