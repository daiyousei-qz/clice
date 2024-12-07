#pragma once

#include <llvm/Support/Path.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/MemoryBuffer.h>

namespace clice {

namespace vfs = llvm::vfs;
namespace fs = llvm::sys::fs;
namespace path = llvm::sys::path;

class ThreadSafeFS : public vfs::ProxyFileSystem {
public:
    explicit ThreadSafeFS() : ProxyFileSystem(vfs::createPhysicalFileSystem()) {}

    class VolatileFile : public vfs::File {
    public:
        VolatileFile(std::unique_ptr<vfs::File> Wrapped) : wrapped(std::move(Wrapped)) {
            assert(this->wrapped);
        }

        llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> getBuffer(const llvm::Twine& Name,
                                                                     int64_t FileSize,
                                                                     bool RequiresNullTerminator,
                                                                     bool /*IsVolatile*/) override {
            return wrapped->getBuffer(Name,
                                      FileSize,
                                      RequiresNullTerminator,
                                      /*IsVolatile=*/true);
        }

        llvm::ErrorOr<vfs::Status> status() override {
            return wrapped->status();
        }

        llvm::ErrorOr<std::string> getName() override {
            return wrapped->getName();
        }

        std::error_code close() override {
            return wrapped->close();
        }

    private:
        std::unique_ptr<File> wrapped;
    };

    llvm::ErrorOr<std::unique_ptr<vfs::File>> openFileForRead(const llvm::Twine& InPath) override {
        llvm::SmallString<128> Path;
        InPath.toVector(Path);

        auto file = getUnderlyingFS().openFileForRead(Path);
        if(!file)
            return file;
        // Try to guess preamble files, they can be memory-mapped even on Windows as
        // clangd has exclusive access to those and nothing else should touch them.
        llvm::StringRef filename = path::filename(Path);
        if(filename.starts_with("preamble-") && filename.ends_with(".pch")) {
            return file;
        }
        return std::make_unique<VolatileFile>(std::move(*file));
    }
};

}  // namespace clice
