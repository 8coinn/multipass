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

#ifndef MULTIPASS_DEFAULT_VM_IMAGE_VAULT_H
#define MULTIPASS_DEFAULT_VM_IMAGE_VAULT_H

#include <multipass/path.h>
#include <multipass/query.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_vault.h>

#include <QDir>
#include <memory>
#include <unordered_map>

namespace multipass
{
class URLDownloader;
class VMImageHost;
class VaultRecord
{
public:
    multipass::VMImage image;
    multipass::Query query;
};
class DefaultVMImageVault final : public VMImageVault
{
public:
    DefaultVMImageVault(VMImageHost* image_host, URLDownloader* downloader, multipass::Path cache_dir_path);
    VMImage fetch_image(const FetchType& fetch_type, const Query& query, const PrepareAction& prepare,
                        const ProgressMonitor& monitor) override;

private:
    void persist_image_records();
    void persist_instance_records();
    void expunge_invalid_image_records();

    VMImageHost* const image_host;
    URLDownloader* const url_downloader;
    const QDir cache_dir;

    std::unordered_map<std::string, VaultRecord> prepared_image_records;
    std::unordered_map<std::string, VaultRecord> instance_image_records;
};
}
#endif // MULTIPASS_DEFAULT_VM_IMAGE_VAULT_H
