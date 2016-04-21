#ifndef AIW_IOSTREAM_HPP
#define AIW_IOSTREAM_HPP

#include <cstdio>
#include <string>
#include "debug.hpp"
#include "mem.hpp"

namespace aiw{
//------------------------------------------------------------------------------
	template <typename S> void format2stream(S &&str, const char *format){
		for(int i=0; format[i]; ++i)
			if(format[i]=='%' && format[i+1]){
				if(format[i+1]!='%') AIW_RAISE("illegal '%' count ", format, i);
				else{ str.write(format, i); format += i+1; i = 0; }
			}
		str<<format;
	}
	template <typename S, typename T, typename ... Args>
	void format2stream(S &&str, const char *format, T x, Args ... args){
		for(int i=0; true; ++i){
			if(format[i]==0) AIW_RAISE("illegal '%' count ", format, i);
			if(format[i]=='%' && format[i+1]){
				if(format[i+1]!='%'){
					if(i) str.write(format, i);
					str<<x; format2stream(str, format+i+1, args...);
					break;
				} else { str.write(format, i); format += i+1; i = 0; }
			}
		}
	}
//------------------------------------------------------------------------------
	struct IOstream{
		std::string name;
		virtual ~IOstream(){}
		
		virtual void close() = 0;
		virtual size_t tell() const = 0;
		virtual void seek(size_t offset, int whence=0) = 0;
		virtual size_t fread(void* buf, size_t size) = 0; // add swig typemap for bind to python style?
		virtual size_t write(const void* buf, size_t size) = 0;
		virtual void flush() = 0;
		
#ifndef SWIG
		virtual std::shared_ptr<BaseAlloc> mmap(size_t size, bool write_mode=false) = 0;
		virtual int printf(const char * format, ...) = 0;
		template <typename ... Args> IOstream &operator ()(const char *format, Args ... args){
			format2stream(*this, format, args...); return *this;
		}
#endif //SWIG

		aiw::IOstream& operator << (const char* S){ write(S, strlen(S)); return *this; }
		aiw::IOstream& operator << (bool   v){ write(v?"True":"False", v?4:5); return *this; }
		aiw::IOstream& operator << (char   c){ printf("%c", c); return *this; }
		aiw::IOstream& operator << (int    v){ printf("%i", v); return *this; }
		aiw::IOstream& operator << (long   v){ printf("%i", v); return *this; }
		aiw::IOstream& operator << (float  v){ printf("%f", v); return *this; }
		aiw::IOstream& operator << (double v){ printf("%lf", v); return *this; }
		// aiw::IOstream& operator << (IOstream& (*func)(IOstream&)){ return func(*this); }
	};
//------------------------------------------------------------------------------
	class File: public IOstream{
		std::shared_ptr<FILE> pf;
	public:
		File(){}
#ifndef SWIG
		template <typename ... Args> open(const char *format, const char *mode, Args ... args){
			stringstream path; format2stream(buf, args...); FILE *f = fopen(path.str().c_str(), mode);
			if(!f) AIW_RAISE("cannot open file", path.str(), mode);
			name = path.str(); pf.reset(f, fclose);
		}
		template <typename ... Args> File(const char *format, const char *mode, Args ... args){ open(format, mode, args...); }
#endif //SWIG
		File(const char *path, const char *mode){ open(path, mode); }
		
		void close(){ pf.reset(); } 
		size_t tell() const { retrurn ftell(pf.get()); }
		void seek(size_t offset, int whence=0){ fseek(pf.get(), offset, whence); }
		size_t fread(void* buf, size_t size){ ::fread(buf, size, 1, pf.get()); }
		size_t write(const void* buf, size_t size){ ::fwrite(buf, size, 1, pf.get()); }
		void flush(){ fflush(pf.get()); }
		std::shared_ptr<BaseAlloc> mmap(size_t size, bool write_mode=false){
			return new MMapAlloc(pf, size, write_mode?O_RDONLY|O_WRONLY:O_RDONLY);
		}
		int printf(const char * format, ...){
			va_list args; va_start(args,format);   
			int r = vfprintf(pf.get(), format, args);
			va_end(args);
			return r;
		}		
	};	
//------------------------------------------------------------------------------
};
#endif //AIW_IOSTREAM_HPP
