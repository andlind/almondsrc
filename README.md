# almondsrc

Almond project source code, without all the bloath of integrations and demos and build stuff.

Almond, (Andreas Lindell Monitoring Daemon) is a tool to bridge old style monitoring tools with new OTEL friendly monitoring pipelines.
Almond used Nagios plugins in an internal scheduler and produces OTEL metrics, Prometheus prom files and Kafka topics. The main program is
written in C but a helper module is written in Python. Both Python and C project parts have a Json API.

Almond is about to bring old style monitoring into the new world and to the same extent bring the modern OTEL monitoring back to monitoring tools.
The bridge between the two monitoring worlds is spelled Almond. Almond is not a complete monitoring system, it is an agent that connects to
other main stream projects.

Use cases is for examples, to bring legacy software without exports or OTEL connections into that category with Almond as a translator. But also
to get all those metrics in the Otel world into legacy monitoring plattforms that were made for monitoring rather than just metrics.

CONTRIBUTION

I´d love to have contributers!
Check the code, help it get better!
Test the product, are there any strange bugs?
Or donate to make the project raise to the level where it should be :)
