#include "duckdb/common/virtual_file_system.hpp"

#include "duckdb/common/gzip_file_system.hpp"
#include "duckdb/common/pipe_file_system.hpp"
#include "duckdb/common/string_util.hpp"

namespace duckdb {

VirtualFileSystem::VirtualFileSystem() : default_fs(FileSystem::CreateLocal()) {
	VirtualFileSystem::RegisterSubSystem(FileCompressionType::GZIP, make_uniq<GZipFileSystem>());
}

unique_ptr<FileHandle> VirtualFileSystem::TryOpenFile(const string &path, uint8_t flags, FileLockType lock,
                                                      FileCompressionType compression, optional_ptr<FileOpener> opener,
                                                      optional_ptr<string> out_error) {
	if (compression == FileCompressionType::AUTO_DETECT) {
		// auto detect compression settings based on file name
		auto lower_path = StringUtil::Lower(path);
		if (StringUtil::EndsWith(lower_path, ".gz")) {
			compression = FileCompressionType::GZIP;
		} else if (StringUtil::EndsWith(lower_path, ".zst")) {
			compression = FileCompressionType::ZSTD;
		} else {
			compression = FileCompressionType::UNCOMPRESSED;
		}
	}
	// open the base file handle
	auto file_handle =
	    FindFileSystem(path)->TryOpenFile(path, flags, lock, FileCompressionType::UNCOMPRESSED, opener, out_error);
	if (!file_handle) {
		return file_handle;
	}
	if (file_handle->GetType() == FileType::FILE_TYPE_FIFO) {
		file_handle = PipeFileSystem::OpenPipe(std::move(file_handle));
	} else if (compression != FileCompressionType::UNCOMPRESSED) {
		auto entry = compressed_fs.find(compression);
		if (entry == compressed_fs.end()) {
			throw NotImplementedException(
			    "Attempting to open a compressed file, but the compression type is not supported");
		}
		file_handle = entry->second->OpenCompressedFile(std::move(file_handle), flags & FileFlags::FILE_FLAGS_WRITE);
	}
	return file_handle;
}

unique_ptr<FileHandle> VirtualFileSystem::OpenFile(const string &path, uint8_t flags, FileLockType lock,
                                                   FileCompressionType compression, FileOpener *opener) {
	string error;
	auto file_handle = TryOpenFile(path, flags, lock, compression, opener, &error);
	if (!file_handle) {
		throw IOException(error);
	}
	return file_handle;
}

} // namespace duckdb
