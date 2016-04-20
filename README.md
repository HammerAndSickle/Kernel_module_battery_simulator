# Kernel_module_battery_simulator
Left battery level checking simulator with kernel module

sudo make


sudo insmod batt_module.ko : loading module

sudo sh test.sh : running simulator

sudo ./battui : battery level UI

sudo ./battma : battery manager (pid to receive signal or threshold about sleep mode)

