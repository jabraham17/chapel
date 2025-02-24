#!/usr/bin/env bash
#
# Test HDFS and Curl package modules

UTIL_CRON_DIR=$(cd $(dirname ${BASH_SOURCE[0]}) ; pwd)
source $UTIL_CRON_DIR/common.bash
source $UTIL_CRON_DIR/common-quickstart.bash

export HADOOP_HOME=/hpcdc/project/chapel/hadoop/$(hostname -s)
export CLASSPATH=$(${HADOOP_HOME}/bin/hadoop classpath --glob)
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HADOOP_HOME/lib/native:$JAVA_HOME/lib:$JAVA_HOME/lib/amd64/server

# remove storage directory root
rm -rf /tmp/hadoop-$USER/

#reformat hdfs filesystem
$HADOOP_HOME/bin/hdfs namenode -format test
#start hdfs
$HADOOP_HOME/sbin/start-dfs.sh
$HADOOP_HOME/bin/hdfs dfsadmin -safemode leave
# make sure files are owned by the user
$HADOOP_HOME/bin/hdfs dfs -chown -R $USER /

export CHPL_NIGHTLY_TEST_CONFIG_NAME="networking-packages"
export CHPL_NIGHTLY_TEST_DIRS="library/packages/Curl library/packages/HDFS library/packages/URL"

$UTIL_CRON_DIR/nightly -cron ${nightly_args}

# clean up test files
$HADOOP_HOME/bin/hdfs dfs -rm -r -f /tmp

#stop hdfs
$HADOOP_HOME/sbin/stop-dfs.sh
