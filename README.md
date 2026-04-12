# almondsrc

Almond is a bridge between the old and new monitoring worlds. It can bring old legacy systems monitored with Nagios plugins right in to the OTEL world.
And at the same time help structure all those OTEL metrics into a legacy monitoring platform.

Almond´s base program is written in C and it has a helper program written in Python Flask. It runs Nagios plugins in an internal scheduler and exposed the results as
JSON (through APIS), HTML, OTEL exporter, Kafka producer, Prometheus metrics. The application is lightweight and very efficient.

INSTALLING WITH AUTOMAKE
   Make sure everyting is clean between builds, running autoreconf -fi
   Other than that is just to run the regular configure, make, make install commands.
   Configure can be run in three ways:
   ./configure --prefix=/opt/almond		 	standard installation of Almond with Kafka support
   ./configure --prefix=/opt/almond --enable-avro	installs Almond with avro support for Kafka
   ./configure --prefix=/opt/almond --disable-kafka	installs a minimal Almond without support for Kafka

CONTRIBUTE

I´d love to hace contributers and some stars at this project.
If you test it, have issues or feature requests - that for one is a great way of contributing.
If you are a coder, I´d love some more people to dig into the code having more ideas or improvement proposals.
This project is GPL3 on true open source is nature. Everyone is welcome to contribute!
You are of course also welcomed as a doner, if you want to help the project grow to the true capacaties of its design.
