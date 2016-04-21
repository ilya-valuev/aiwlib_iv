#ifndef AIW_GZSTREAM_HPP
#define AIW_GZSTREAM_HPP

#include <zlib.h>
#include "iostream.hpp"

namespace aiw{
//------------------------------------------------------------------------------
	class GzFile: public IOstream {
		std::shared_ptr<gzFile_s> pf;
	public:
		GzFile(){}
#ifndef SWIG
		template <typename ... Args> open(const char *format, const char *mode, Args ... args){
			stringstream path; format2stream(buf, args...); gzFile f = gzopen(path.str().c_str(), mode);
			if(!f) AIW_RAISE("cannot open file", path.str(), mode);
			name = path.str(); pf.reset(f, gzclose);
		}
		template <typename ... Args> GzFile(const char *format, const char *mode, Args ... args){ open(format, mode, args...); }
#endif //SWIG
		GzFile(const char *path, const char *mode){ open(path, mode); }
		
		void close(){ pf.reset(); } 
		size_t tell() const { retrurn gztell(pf.get()); }
		void seek(size_t offset, int whence=0){ gzseek(pf.get(), offset, whence); }
		size_t fread(void* buf, size_t size){ gzread(pf.get(), buf, size); }
		size_t write(const void* buf, size_t size){ gzwrite(pf.get(), buf, size); }
		void flush(){} // ???
		std::shared_ptr<BaseAlloc> mmap(size_t size, bool write_mode=false){
			if(write_mode) AIW_RAISE("cann't mmapped gzfile to write mode", name); // ???
			MemAlloc<char>* ptr = new MemAlloc<char>(size); gzread(pf.get(), ptr, size);
			return ptr;
		}
		int printf(const char * format, ...){
			va_list args; va_start(args, format);   
			int r = gzprintf(pf.get(), format, args);
			va_end(args);
			return r;
		}		

	};
//------------------------------------------------------------------------------
};
#endif //AIW_GZSTREAM_HPP
