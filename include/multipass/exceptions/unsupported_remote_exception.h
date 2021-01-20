/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#ifndef MULTIPASS_UNSUPPORTED_REMOTE_EXCEPTION_H
#define MULTIPASS_UNSUPPORTED_REMOTE_EXCEPTION_H

#include <stdexcept>

#include <multipass/format.h>

namespace multipass
{
class UnsupportedRemoteException : public std::runtime_error
{
public:
    UnsupportedRemoteException(const std::string& message) : runtime_error(message)
    {
    }
};
} // namespace multipass
#endif // MULTIPASS_UNSUPPORTED_REMOTE_EXCEPTION_H