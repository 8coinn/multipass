/*
 * Copyright (C) 2019 Canonical, Ltd.
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
 */

#ifndef MULTIPASS_LINUXPROCESS_H
#define MULTIPASS_LINUXPROCESS_H

#include "process_spec.h"
#include <QProcess>
#include <memory>
#include <multipass/process.h>

namespace multipass
{

class LinuxProcess : public Process
{
    Q_OBJECT
public:
    virtual ~LinuxProcess() = default;

    QString program() const override;
    QStringList arguments() const override;
    QString working_directory() const override;
    QProcessEnvironment process_environment() const override;

    void start() override;
    void kill() override;

    multipass::optional<ProcessExitState>
    wait_for_started(int msecs = 30000) override; // optional set if process fails to start
    const ProcessExitState
    wait_for_finished(int msecs = 30000) override; // optional set if wait times-out, or process never started

    bool running() const override;

    QByteArray read_all_standard_output() override;
    QByteArray read_all_standard_error() override;

    qint64 write(const QByteArray& data) override;
    void close_write_channel() override;

    const ProcessExitState execute(const int timeout = 30000) override;

protected:
    LinuxProcess(std::unique_ptr<ProcessSpec>&& spec);
    const std::unique_ptr<ProcessSpec> process_spec;

    QProcess process; // ease testing
};

} // namespace multipass

#endif // MULTIPASS_LINUXPROCESS_H
