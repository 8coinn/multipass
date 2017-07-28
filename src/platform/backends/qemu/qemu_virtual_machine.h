/*
 * Copyright (C) 2017 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_QEMU_VIRTUAL_MACHINE_H
#define MULTIPASS_QEMU_VIRTUAL_MACHINE_H

#include <multipass/virtual_machine.h>

class QProcess;
class QFile;
class QString;

namespace multipass
{
class VMStatusMonitor;
class VirtualMachineDescription;

class QemuVirtualMachine final : public VirtualMachine
{
public:
    QemuVirtualMachine(const VirtualMachineDescription& desc, int ssh_fowarding_port, VMStatusMonitor& monitor);
    ~QemuVirtualMachine();

    void start() override;
    void stop() override;
    void shutdown() override;
    State current_state() override;
    int forwarding_port() override;
    std::string host() override;
    void wait_until_ssh_up(std::chrono::milliseconds timeout) override;

private:
    void on_started();
    void on_error();
    void on_shutdown();
    VirtualMachine::State state;
    int ssh_fowarding_port;
    VMStatusMonitor* monitor;
    std::unique_ptr<QProcess> vm_process;
};
}

#endif // MULTIPASS_QEMU_VIRTUAL_MACHINE_H
