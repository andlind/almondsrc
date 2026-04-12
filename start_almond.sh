#!/bin/bash
#  ##     ####     ##   ##   ## ##   ###  ##  ### ##  #
#   ##     ##       ## ##   ##   ##    ## ##   ##  ## #
# ## ##    ##      # ### #  ##   ##   # ## #   ##  ## #
# ##  ##   ##      ## # ##  ##   ##   ## ##    ##  ## #
# ## ###   ##      ##   ##  ##   ##   ##  ##   ##  ## #
# ##  ##   ##  ##  ##   ##  ##   ##   ##  ##   ##  ## #
####  ##  ### ###  ##   ##   ## ##   ###  ##  ### ##  #
#
#   Almond Monitoring Daemon start script
#   Author: Andreas Lindell <Andreas_li@hotmail.com
#
ALMOND_PROC=/opt/almond/bin/almond
if test -f "$ALMOND_PROC"; then
	exec /opt/almond/bin/almond
else
	echo "Almond is not installed"
fi
