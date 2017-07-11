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

#include "default_vm_image_vault.h"
#include "json_writer.h"

#include <multipass/query.h>
#include <multipass/url_downloader.h>
#include <multipass/vm_image.h>
#include <multipass/vm_image_host.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <exception>

namespace mp = multipass;

namespace
{
constexpr char instance_db_name[] = "multipassd-instance-image-records.json";
constexpr char image_db_name[] = "multipassd-image-records.json";

auto filename_for(const QString& path)
{
    QFileInfo file_info(path);
    return file_info.fileName();
}

auto query_to_json(const mp::Query& query)
{
    QJsonObject json;
    json.insert("release", QString::fromStdString(query.release));
    json.insert("persistent", query.persistent);
    return json;
}

auto image_to_json(const mp::VMImage& image)
{
    QJsonObject json;
    json.insert("path", image.image_path);
    json.insert("kernel_path", image.kernel_path);
    json.insert("initrd_path", image.initrd_path);
    json.insert("id", QString::fromStdString(image.id));
    return json;
}

auto record_to_json(const mp::VaultRecord& record)
{
    QJsonObject json;
    json.insert("image", image_to_json(record.image));
    json.insert("query", query_to_json(record.query));
    return json;
}

std::unordered_map<std::string, mp::VaultRecord> load_db(const QString& db_name)
{
    QFile db_file{db_name};
    auto opened = db_file.open(QIODevice::ReadOnly);
    if (!opened)
        return {};

    QJsonParseError parse_error;
    auto doc = QJsonDocument::fromJson(db_file.readAll(), &parse_error);
    if (doc.isNull())
        return {};

    auto records = doc.object();
    if (records.isEmpty())
        return {};

    std::unordered_map<std::string, mp::VaultRecord> reconstructed_records;
    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        auto key = it.key().toStdString();
        auto record = it.value().toObject();
        if (record.isEmpty())
            return {};

        auto image = record["image"].toObject();
        if (image.isEmpty())
            return {};

        auto image_path = image["path"].toString();
        if (image_path.isNull())
            return {};

        auto kernel_path = image["kernel_path"].toString();
        auto initrd_path = image["initrd_path"].toString();
        auto image_id = image["id"].toString().toStdString();

        auto query = record["query"].toObject();
        if (query.isEmpty())
            return {};

        auto release = query["release"].toString();
        auto persistent = query["persistent"];
        if (!persistent.isBool())
            return {};

        reconstructed_records[key] = {{image_path, kernel_path, initrd_path, image_id},
                                      {"", release.toStdString(), persistent.toBool()}};
    }
    return reconstructed_records;
}

QString copy(const QString& file_name, const QDir& output_dir)
{
    if (file_name.isEmpty())
        return {};

    QFileInfo info{file_name};
    const auto source_name = info.fileName();
    auto new_path = output_dir.filePath(source_name);
    QFile::copy(file_name, new_path);
    return new_path;
}

void delete_file(const QString& path)
{
    QFile file{path};
    file.remove();
}

void remove_source_images(const mp::VMImage& source_image, const mp::VMImage& prepared_image)
{
    // The prepare phase may have been a no-op, check and only remove source images
    if (source_image.image_path != prepared_image.image_path)
        delete_file(source_image.image_path);
    if (source_image.kernel_path != prepared_image.kernel_path)
        delete_file(source_image.kernel_path);
    if (source_image.initrd_path != prepared_image.initrd_path)
        delete_file(source_image.initrd_path);
}

QDir make_dir(const QString& name, const QDir& cache_dir)
{
    if (!cache_dir.mkdir(name))
    {
        qDebug() << "make_dir: name: " << name;
        QString dir{cache_dir.filePath(name)};
        throw std::runtime_error("unable to create directory \'" + dir.toStdString() + "\'");
    }

    QDir new_dir{cache_dir};
    new_dir.cd(name);
    return new_dir;
}

class DeleteOnException
{
public:
    DeleteOnException(const mp::Path& path) : file(path)
    {
    }
    ~DeleteOnException()
    {
        if (std::uncaught_exception())
        {
            file.remove();
        }
    }

private:
    QFile file;
};
}

mp::DefaultVMImageVault::DefaultVMImageVault(VMImageHost* image_host, URLDownloader* downloader,
                                             multipass::Path cache_dir_path)
    : image_host{image_host},
      url_downloader{downloader},
      cache_dir{cache_dir_path},
      prepared_image_records{load_db(cache_dir.filePath(image_db_name))},
      instance_image_records{load_db(cache_dir.filePath(instance_db_name))}
{
}

mp::VMImage mp::DefaultVMImageVault::fetch_image(const FetchType& fetch_type, const Query& query,
                                                 const PrepareAction& prepare, const ProgressMonitor& monitor)
{
    auto name_entry = instance_image_records.find(query.name);
    if (name_entry != instance_image_records.end())
    {
        const auto& record = name_entry->second;
        return record.image;
    }

    auto info = image_host->info_for(query);
    auto id = info.id.toStdString();

    auto it = prepared_image_records.find(id);
    if (it != prepared_image_records.end())
    {
        const auto& record = it->second;
        const auto prepared_image = record.image;
        auto vm_image = image_instance_from(query.name, prepared_image);
        instance_image_records[query.name] = {vm_image, query};
        persist_instance_records();
        return vm_image;
    }

    QString image_dir_name = QString("%1-%2").arg(info.release).arg(info.version);
    auto image_dir = make_dir(image_dir_name, cache_dir);

    VMImage source_image;
    source_image.id = id;
    source_image.image_path = image_dir.filePath(filename_for(info.image_location));
    DeleteOnException image_file{source_image.image_path};

    url_downloader->download_to(info.image_location, source_image.image_path, monitor);

    if (fetch_type == FetchType::ImageKernelAndInitrd)
    {
        source_image.kernel_path = image_dir.filePath(filename_for(info.kernel_location));
        source_image.initrd_path = image_dir.filePath(filename_for(info.initrd_location));
        DeleteOnException kernel_file{source_image.kernel_path};
        DeleteOnException initrd_file{source_image.initrd_path};
        url_downloader->download_to(info.kernel_location, source_image.kernel_path, monitor);
        url_downloader->download_to(info.initrd_location, source_image.initrd_path, monitor);
    }

    auto prepared_image = prepare(source_image);
    prepared_image_records[id] = {prepared_image, query};
    remove_source_images(source_image, prepared_image);

    auto vm_image = image_instance_from(query.name, prepared_image);
    instance_image_records[query.name] = {vm_image, query};

    expunge_invalid_image_records();
    persist_image_records();
    persist_instance_records();

    return vm_image;
}

void mp::DefaultVMImageVault::remove(const std::string& name)
{
    auto name_entry = instance_image_records.find(name);
    if (name_entry == instance_image_records.end())
        return;

    const auto& record = name_entry->second;
    delete_file(record.image.image_path);
    delete_file(record.image.kernel_path);
    delete_file(record.image.initrd_path);
    cache_dir.rmdir(QString::fromStdString(name));

    instance_image_records.erase(name);
    persist_instance_records();
}

mp::VMImage mp::DefaultVMImageVault::image_instance_from(const std::string& instance_name,
                                                         const VMImage& prepared_image)
{
    auto name = QString::fromStdString(instance_name);
    auto output_dir = make_dir(name, cache_dir);

    return {copy(prepared_image.image_path, output_dir),
            copy(prepared_image.kernel_path, output_dir),
            copy(prepared_image.initrd_path, output_dir), prepared_image.id};
}

void mp::DefaultVMImageVault::persist_instance_records()
{
    QJsonObject instance_records_json;
    for (const auto& record : instance_image_records)
    {
        auto key = QString::fromStdString(record.first);
        instance_records_json.insert(key, record_to_json(record.second));
    }
    write_json(instance_records_json, cache_dir.filePath(instance_db_name));
}

void mp::DefaultVMImageVault::persist_image_records()
{
    QJsonObject image_records_json;
    for (const auto& record : prepared_image_records)
    {
        auto key = QString::fromStdString(record.first);
        image_records_json.insert(key, record_to_json(record.second));
    }
    write_json(image_records_json, cache_dir.filePath(image_db_name));
}

void mp::DefaultVMImageVault::expunge_invalid_image_records()
{
    std::vector<decltype(prepared_image_records)::key_type> invalid_keys;
    for (const auto& record : prepared_image_records)
    {
        const auto& key = record.first;
        auto info = image_host->info_for(record.second.query);
        if (info.id.toStdString() != key)
            invalid_keys.push_back(key);
    }

    for (auto const& key : invalid_keys)
        prepared_image_records.erase(key);
}
