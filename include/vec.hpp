#ifndef AIW_VEC_HPP
#define AIW_VEC_HPP
/**
 * Copyright (C) 2009-2016 Antov V. Ivanov, KIAM RAS, Moscow.
 * This code is released under the GPL2 (GNU GENERAL PUBLIC LICENSE Version 2, June 1991)
 **/

#ifdef SWIGPYTHON
inline void push_vec_data(void *obj, int offset, const char* data, int size){
	for(int i=0; i<size; i++) ((char*)obj)[offset+i] = data[i];
}
inline PyObject* pull_vec_data(void *obj, int offset, int size){
	return PyString_FromStringAndSize( ((const char*)obj)+offset, size);
}
inline int swig_types_table_size(){ return swig_module.size; }
inline const char* swig_types_table_get_item(int i){ return swig_types[i]->str; }
inline void swig_types_table_patch(int src, int dst){ swig_types[src]->clientdata = swig_types[dst]->clientdata; }
inline const char* get_swig_type(PyObject* obj){ return ((SwigPyObject*)obj)->ty->str; }
inline void set_swig_type(PyObject* obj, int T){ ((SwigPyObject*)obj)->ty = swig_types[T]; }

class PVec{ char p[1024]; };
#endif //SWIGPYTHON

#include <math.h>
// #include "debug.hpp"

namespace aiw{
//---------------------------------------------------------------------------
#ifndef SWIG
	template <int D, typename T=double> class Vec{
		T p[D];

		inline void set_x(){}
		template <typename T2, typename ... Args>
		inline void set_x(const T2& x, const Args&... xxx){	p[D-1-sizeof...(Args)] = x; set_x(xxx...); }
	public:
        explicit Vec(T val=0) { for(int i=0; i<D; i++) p[i] = val; }    
		// ??? explicit Vec(const T* Ap)      { for(int i=0; i<D; i++) p[i] = Ap[i]; }    
		Vec(const Vec<1, T> &v){ for(int i=0; i<D; i++) p[i] = v[0]; }
		template <class T2> /* explicit ??? */ Vec(const Vec<D, T2> &v){ for(int i=0; i<D; i++) p[i] = v[i]; }

		template <typename ... Args> explicit Vec(const Args&... xxx){ 
			static_assert(sizeof...(Args)==D, "illegal parametrs count!"); 
			set_x(xxx...); 
		}
		template <class T2> inline Vec& operator = (const Vec<D,T2> &v){ for(int i=0; i<D; i++) p[i] = v[i]; return *this; }

		inline T  operator [] (int i) const { 
			aiw_assert(0<=i && i<D, "Vec<%i, szT=%i>::operator [] --- index %i out of range", D, sizeof(T), i); 
			return p[i]; 
		} 
		inline T& operator [] (int i) { 
			aiw_assert(0<=i && i<D, "Vec<%i, szT=%i>::operator [] --- index %i out of range", D, sizeof(T), i); 
			return p[i]; 
		} 
		

		inline Vec operator -() const { Vec res; for(int i=0; i<D; i++) res[i] = -p[i]; return res; }

#define VEC_BIN_OP(ROP, OP, RES)										\
		template <typename T2>											\
		inline void operator OP= (const Vec<D,T2> &v){					\
			for(int i=0; i<D; i++) p[i] = RES;							\
		}																\
		template <typename T2>											\
		inline Vec<D, decltype(T() ROP T2())>							\
		operator OP (const Vec<D,T2> &v) const {						\
			decltype(*this ROP v) r;									\
			for(int i=0; i<D; i++) r[i] = RES;							\
			return r;													\
		}
		VEC_BIN_OP(+, +, p[i]+v[i]);
		VEC_BIN_OP(-, -, p[i]-v[i]);
		VEC_BIN_OP(/, /, p[i]/v[i]);
		VEC_BIN_OP(*, &, p[i]*v[i]);
		// VEC_BIN_OP(+, ^, p[i]&&v[i]?0:(p[i]?p[i]:v[i]))
#undef  VEC_BIN_OP

		template <typename T2> inline bool operator < (const Vec<D,T2> &v) const {
			for(int i=0; i<D; i++) if(p[i]>=v[i]) return false; return true;
		}																
		template <typename T2> inline bool operator <= (const Vec<D,T2> &v) const {
			for(int i=0; i<D; i++) if(p[i]>v[i]) return false; return true;
		}																
		template <typename T2> inline bool operator > (const Vec<D,T2> &v) const {
			for(int i=0; i<D; i++) if(p[i]<=v[i]) return false; return true;
		}																
		template <typename T2> inline bool operator >= (const Vec<D,T2> &v) const {
			for(int i=0; i<D; i++) if(p[i]<v[i]) return false; return true;
		}																
		template <typename T2> inline bool operator == (const Vec<D,T2> &v) const { // округление ???
			for(int i=0; i<D; i++) if(p[i]!=v[i]) return false; return true;
		}																
		template <typename T2> inline bool operator != (const Vec<D,T2> &v) const { // округление ???
			for(int i=0; i<D; i++) if(p[i]!=v[i]) return true; return false;
		}																

		template <typename T2> inline void operator *= (const T2 &x){ for(int i=0; i<D; i++) p[i] *= x;	}
		template <typename T2> inline void operator /= (const T2 &x){ *this *= 1./x; }

		inline Vec  operator <<  (const Vec &v) const { Vec r; for(int i=0; i<D; i++) r[i] = p[i]<v[i]?p[i]:v[i]; return r; } 
		inline void operator <<= (const Vec &v)       {        for(int i=0; i<D; i++) p[i] = p[i]<v[i]?p[i]:v[i]; } 
		inline Vec  operator >>  (const Vec &v) const { Vec r; for(int i=0; i<D; i++) r[i] = p[i]>v[i]?p[i]:v[i]; return r; } 
		inline void operator >>= (const Vec &v)       {        for(int i=0; i<D; i++) p[i] = p[i]>v[i]?p[i]:v[i]; } 
		
		inline T  periodic(int i) const { i %= D; if(i<0) i+= D; return p[i]; } 
		inline T& periodic(int i)       { i %= D; if(i<0) i+= D; return p[i]; } 
		inline Vec circ(int l) const { // cyclic shift
			Vec res; l %= D;
			for(int i=0; i<l; i++) res.p[i] = p[D-l+i]; 
			for(int i=l; i<D; i++) res.p[i] = p[i-l]; 
			return res; 
		}

		template <typename ... Args> 
		inline Vec<sizeof ...(Args), T> operator ()(Args ... xxx) const { 
			return Vec<sizeof ...(Args), T>((*this)[xxx]...);
		}

		// *** OTHER FUNCTIONS ***
		inline T len() const { T x = 0.; for(int i=0; i<D; i++) x += p[i]*p[i]; return sqrt(x); } // abs ???
		inline Vec pow(const int n) const { // ????
			if(n==0) return Vec((T)1.);		
			if(n==1) return *this;
			Vec v, res; for(int i=0; i<D; i++) v[i] = n>0 ? p[i] : 1./p[i];
	 		const int abs_n = n>0 ? n : -n;
	    	int k = -1;	while( !(abs_n&(1<<++k)) && k<31 ) v ^= v;
			res = v;
			while( abs_n>>++k && k<31 ){ v ^= v; if(abs_n&(1<<k)) res &= v; }
     		return res;		
		}
		inline Vec pow(double n) const { Vec v; for(int i=0; i<D; i++) v[i] = ::pow(p[i], n); return v; }
		inline Vec mod(T x) const { Vec v; for(int i=0; i<D; i++) v[i] = p[i]%x; return v; }

		inline Vec fabs() const { Vec v; for(int i=0; i<D; i++) v[i] = ::fabs(p[i]); return v; }
		inline Vec ceil() const { Vec v; for(int i=0; i<D; i++) v[i] = ::ceil(p[i]); return v; }
		inline Vec floor() const { Vec v; for(int i=0; i<D; i++) v[i] = ::floor(p[i]); return v; }
		inline Vec round() const { Vec v; for(int i=0; i<D; i++) v[i] = ::round(p[i]); return v; }

		inline T min() const { T res = p[0]; for(int i=1; i<D; i++) res = res<p[i]?res:p[i]; return res; }
		inline T max() const { T res = p[0]; for(int i=1; i<D; i++) res = res>p[i]?res:p[i]; return res; }
		inline T sum() const { T res = p[0]; for(int i=1; i<D; i++) res += p[i]; return res; }
		inline T prod() const { T res = p[0]; for(int i=1; i<D; i++) res *= p[i]; return res; }

		/*
		inline bool is_nan() const { 
			for(int i=0; i<D; i++) if( isnan(p[i]) ) return true;
			return false;
		}
		inline bool is_bad() const { 
			for(int i=0; i<D; i++) if( ! isnormal(p[i]) ) return true;
			return false;
		}

		inline const char* c_str(const char* separator=" ", const char* format="%g") const { //return c_strV( D, p, format, separator ); } //???
			buf_open(); buf_print(format, (double)p[0]);  
			for(int i=1; i<D; i++){ buf_print("%s", separator); buf_print(format, (double)p[i]); } //???
			return buf_close();  
		}
		*/
		
	}; // end of class Vec
//---------------------------------------------------------------------------
//   EXTEND OPERATIONS
//---------------------------------------------------------------------------
	template <typename T, typename ... Args> 
	inline Vec<sizeof...(Args)+1, T> vec(T x, Args ... args){
		return Vec<sizeof...(Args)+1, T>(x, args...); 
	}
//---------------------------------------------------------------------------
	template <int D, typename T1, typename T2>
	inline Vec<D, decltype(T1()*T2())> operator * (const Vec<D, T1> v, const T2 &x){
		decltype(v*x) r;								
		for(int i=0; i<D; i++) r[i] = v[i]*x;
		return r;												   
	}
	template <int D, typename T1, typename T2>
	inline Vec<D, decltype(T1()*T2())> operator / (const Vec<D, T1> v, const T2 &x){ return v*(1./x); }
	template <int D, typename T1, typename T2>
	inline Vec<D, decltype(T1()*T2())> operator * (const T2 &x, const Vec<D, T1> v){
		decltype(x*v) r;								
		for(int i=0; i<D; i++) r[i] = x*v[i];
		return r;												   
	}
	inline Vec<D, decltype(T1()*T2())> operator / (const T2 &x, const Vec<D, T1> v){
		decltype(x/v) r;								
		for(int i=0; i<D; i++) r[i] = x/v[i];
		return r;												   
	}	
	template <int D, typename T1, typename T2> 
	inline decltype(T1()*T2()) operator * (const Vec<D, T1> &a, const Vec<D, T2> &b){
		decltype(T1()*T2()) res = 0.; // ???
		for(int i=0; i<D; i++) res += a[i]*b[i];
		return res;
	}
	template <int D> inline int64_t operator * (const Vec<D, int> &a, const Vec<D, int> &b){
		int64_t res = 0;
		for(int i=0; i<D; i++) res += (int64_t)(a[i])*b[i];
		return res;
	}
	template <int D> inline int64_t Vec<D, int>::prod() const { int64_t res = p[0]; for(int i=1; i<D; i++) res *= p[i]; return res; }
//---------------------------------------------------------------------------
    template <typename T1, typename T2>
	inline decltype(T1()*T2()) operator % (const Vec<2, T1> &a, const Vec<2, T2> &b){ return a[0]*b[1]-a[1]*b[0]; }
    template <typename T1, typename T2>
    inline Vec<3, decltype(T1()*T2())> operator % (const Vec<3, T1> &a, const Vec<3, T2> &b){
		Vec<3, decltype(T1()*T2())> c;
		c[0] = a[1]*b[2]-a[2]*b[1];
		c[1] = a[2]*b[0]-a[0]*b[2];
		c[2] = a[0]*b[1]-a[1]*b[0];
		return c;
    }
	/*
    template <typename T1, typename T2>
    inline void operator %= (Vec<3, T1> &a, const Vec<3, T2> &b){
		T1 a0 = a[0], a1 = a[1];
		a[0] = a1*b[2]   - a[2]*b[1];
		a[1] = a[2]*b[0] - a0*b[2];
		a[2] = a0*b[1]   - a1*b[0];
		return a;
    }
	*/
//---------------------------------------------------------------------------
    template <int D, class T1, class T2> inline Vec<D+1, decltype(T1()+T2())> operator | (const T1 &x, const Vec<D, T2> &v){ 
		Vec<D+1, decltype(T1()+T2())> r; r[0] = x; 
		for(int i=0; i<D; i++) r[i+1] = v[i]; 
		return r; 
	}
    template <int D, class T1, class T2> inline Vec<D+1, decltype(T1()+T2())> operator | (const Vec<D, T1> &r, const T2 &x){ 
		Vec<D+1, decltype(T1()+T2())> v; 
		for(int i=0; i<D; i++) v[i] = r[i]; 
		v[D] = x; return v; 
	}
    template <int D1, int D2, class T1, class T2> 
	inline Vec<D1+D2,  decltype(T1()+T2())> operator | (const Vec<D1,T1> &r1, const Vec<D2, T2> &r2){ 
		Vec<D1+D2,  decltype(T1()+T2())> v; 
		for(int i=0; i<D1; i++) v[i] = r1[i]; 
		for(int i=0; i<D2; i++) v[D1+i] = r2[i]; 
		return v; 
    }
//---------------------------------------------------------------------------
    template <int D, typename T1, typename T2, typename T3>
	inline double angle(const Vec<D, T1> &a, const Vec<D, T2> &b, const Vec<D,T3> &c){
		auto ab = b-a; auto bc = c-b; ab /= ab.abs(); bc /= bc.abs();
		return acos(ab*bc);
    }
//---------------------------------------------------------------------------
	template<int D> using Ind = Vec<D, int>;
//---------------------------------------------------------------------------
}; // end of namespace aiw

namespace std{
//---------------------------------------------------------------------------
	template<typename T> struct less;
	template<> struct less<double> {
		inline bool operator()(double x, double y) const { 
			int px, py; frexp(x, &px); frexp(y, &py);	
			return y-x > ldexp(1., (px<py?px:py)-40);
		}
	};
	template<> struct less<float> {
		inline bool operator()(float x, float y) const { 
			int px, py; frexp(x, &px); frexp(y, &py);	
			return y-x > ldexp(1., (px<py?px:py)-16);
		}
	};
	template<int D, typename T> struct less<aiw::Vec<D, T> > {
		bool operator()(const aiw::Vec<D, T> &x, const aiw::Vec<D, T> &y ) const { 
			less<T> cmp_l;
			for(int i=D-1; i>=0; i--){
				if(cmp_l(x[i], y[i])) return true;
				if(cmp_l(y[i], x[i])) return false;
			}
			return false;
		} 
	};	
//---------------------------------------------------------------------------
}; // end of namespace std
#endif //SWIG
#endif //AIW_EUCLID_HPP
