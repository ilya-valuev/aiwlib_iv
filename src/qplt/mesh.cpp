/**
 * Copyright (C) 2021 Antov V. Ivanov  <aiv.racs@gmail.com> with support Kintech Lab
 * Licensed under the Apache License, Version 2.0
 **/

#include <omp.h>
#include <sstream>
#include <algorithm>
#include "../../include/aiwlib/interpolations"
#include "../../include/aiwlib/binhead"
#include "../../include/aiwlib/qplt/mesh"
#include "../../include/aiwlib/qplt/mesh_cu"
using namespace aiw;

# ifdef max
# undef max
# endif

# ifdef min
# undef min
# endif


//------------------------------------------------------------------------------
//   load data
//------------------------------------------------------------------------------
bool aiw::QpltMesh::load(IOstream &S){
	// printf("load0: %p\n", mem_ptr);
	BinaryHead bh; size_t s = S.tell();
	if(!bh.load(S) || bh.type!=BinaryHead::mesh || bh.dim>6){ S.seek(s); return false; }
	head = bh.head; dim = bh.dim; szT = bh.szT; logscale = bh.logscale; for(int i=0; i<dim; i++){ bbox[i] = bh.bbox[i]; anames[i] = bh.axis[i]; }
	for(int i=0; i<dim; i++) if(anames[i].empty()) anames[i] = default_anames[i];
	
	/*** for(int i=0; i<dim;) if(bbox0[i]==1) for(int j=i+1; j<dim; j++){  // убираем оси с размером 1
				bbox0[j-1] = bbox0[j]; bmin0[j-1] = bmin0[j]; bmax0[j-1] = bmax0[j]; axes[j-1] = axes[j];
				uint32_t m = ~uint32_t(0)<<i; logscale = (logscale&~m)|((logscale&m)>>1);
				dim--;
				} else i++; ***/
	
	data_sz = szT; for(int i=0; i<dim; i++) data_sz *= bbox[i];
	mem_sz = data_sz/1e9;  fin = S.copy();  mem_offset = S.tell(); if(!S.seek(data_sz, 1)) return false;  // файл битый, записан не до конца
	
	s = S.tell(); int32_t sz2 = 0; S.load(sz2);  // try read old aivlib mesh format (deprecated)
	if(S.tell()-s==4 && sz2==-int(dim*24+4+szT)){ S.read(bh.bmin, dim*8); S.read(bh.bmax, dim*8); S.seek(dim*8, 1); S.seek(szT, 1); logscale = 0;  } 
	else  S.seek(s);
	for(int i=0; i<dim; i++) if(bh.bmin[i]==bh.bmax[i]){  bmin[i] = 0; bmax[i] = bbox[i]; } else {  bmin[i] = bh.bmin[i]; bmax[i] = bh.bmax[i]; }
	calc_step();

	// for(auto i: bf.tinfo.get_access()) std::cout<<i.label<<' '<<i.offset<<'\n';
	// std::cout<<bf.tinfo;
	// #ifdef AIW_TYPEINFO
	//	cfa_list = bf.tinfo.get_access();
	//#endif //AIW_TYPEINFO
	// segy = false;

	mul[0] = szT; for(int i=1; i<dim; i++) mul[i] = mul[i-1]*bbox[i-1];
	// printf("load1: ptr=%p, data_sz=%ld\n", mem_ptr, data_sz);
	return true;
}
void aiw::QpltMesh::data_load_cuda(){ fin->seek(mem_offset); fin->read(mem_ptr, data_sz); }
//------------------------------------------------------------------------------
QpltPlotter* aiw::QpltMesh::mk_plotter(int mode) { this->data_load(); return new QpltMeshPlotter; }
//------------------------------------------------------------------------------
template <int AID>  aiw::QpltMeshFlat<AID>  aiw::QpltMeshPlotter::get_flat(int flatID) const {
	const QpltMesh* cnt = dynamic_cast<const QpltMesh*>(container);
	QpltMeshFlat<AID> f; (QpltFlat&)f = QpltPlotter::get_flat(flatID);  f.acc = (QpltAccessor*)&accessor;  f.interp = 0; // жуткая жуть с const--> не const?
	f.ptr0 = cnt->mem_ptr; for(int i=0; i<cnt->dim; i++) f.ptr0 += cnt->mul[i]*f.spos[i];
	for(int i=0; i<2; i++){
		int a = f.axis[i], a0 = axisID[a];
		f.bbeg[i] = bbeg[a]; f.bbox[i] = bbox[a];
		f.bbox0[i] = cnt->bbox[a0];
		f.step2D[i] = float(f.bbox[i])/im_size[i]; //  cnt->step[a0];
		f.mul[i] = cnt->mul[a0]; // flips ???
		if(flips&(1<<a)){ f.ptr0 += (f.bbox[i]-1)*f.mul[i]; f.mul[i] = -f.mul[i]; }
		f.interp |= ((interp&(3<<f.axis[i]*2))>>2*f.axis[i])<<4*(1-i); // ???
	} // f.axe, f.axe_pos ???
	int a = 3-(f.axis[0]+f.axis[1]), a0 = axisID[a], a_pos = f.spos[a0];  // 0+1=1-->2 || 1+2=3-->0 || 0+2-->1
	f.mul[2] = flips&(1<<a)? -cnt->mul[a0]: cnt->mul[a0];  
	f.diff3minus = flips&(1<<a)? (a_pos+1<cnt->bbox[a0]): (a_pos>0); 
	f.diff3plus  = flips&(1<<a)? (a_pos>0): (a_pos+1<cnt->bbox[a0]);
	// WOUT(f.interp);
	return f;
}
//------------------------------------------------------------------------------
//   implemetation of plotter virtual functions
//------------------------------------------------------------------------------
template <int AID> void aiw::QpltMeshPlotter::init_impl(int autoscale){
	Vecf<2> f_lim(color.get_min(), color.get_max());
	if(autoscale&1){
		QpltMesh* cnt = (QpltMesh*)container; // ???
		Ind<6> smin, smax = cnt->bbox; int dim0 = container->get_dim();
		if(autoscale==1){ // по выделенному кубу
			smin = spos; smax = spos; for(int i=0; i<dim0; i++) smax[i]++;
			for(int i=0; i<dim; i++) smax[axisID[i]] += bbox[i]-1;
		}
		// for(int i=container->dim; i<6; i++) smax[i] = smin[i]+1; ???
		Ind<17> LID = smin|smax|AID|accessor.ctype|Ind<3>(accessor.offsets);
		auto I_LID = cnt->flimits.find(LID); 
		if(I_LID!=cnt->flimits.end()) f_lim = I_LID->second; 
		else { // считаем пределы
#ifdef PROFILE
			double t0 = omp_get_wtime();
#endif //PROFILE
			bool acc_minus = accessor.minus; accessor.minus = false;
			constexpr int DIFF = (AID>>3)&7; // какой то static method в accessor?
			int ddim = accessor.Ddiff(); constexpr int dout = QpltAccessor::DOUT<AID>(); // размерность дифф. оператора  и выходных данных			
			const char* ptr0 = cnt->mem_ptr; for(int i=0; i<dim0; i++) ptr0 += smin[i]*cnt->mul[i];
			float f_min = HUGE_VALF,  f_max = -HUGE_VALF;
      Ind<6> sbox = smax-smin; size_t sz = sbox[0]; for(int i=1; i<cnt->dim; i++) sz *= sbox[i];
#ifndef AIW_WIN32
#pragma omp parallel for reduction(min:f_min) reduction(max:f_max)  // MSVC does not support OpenMP 3
#else //AIW_WIN32      
      std::vector<float> f_mina(omp_get_max_threads(), HUGE_VAL), f_maxa(omp_get_max_threads(), -HUGE_VAL);
      int act_numthreads = -1; // to be determined in the following
#pragma omp parallel for //shared(f_mina, f_maxa)
#endif //AIW_WIN32
			for(int i=0; i<(int)sz; i++){ //for(size_t i=0; i<sz; i++){
				int pos[6]; size_t j = i; const char *ptr1 = ptr0, *nb[6] = {nullptr};   
				for(int k=0; k<dim0; k++){ pos[k] = j%sbox[k]; ptr1 += cnt->mul[k]*pos[k]; pos[k] += smin[k]; j /= sbox[k]; }					
				if(DIFF) for(int k=0; k<ddim; k++){
						int a = axisID[k];
						if(pos[a]>0) nb[2*k] = ptr1-cnt->mul[a]; else nb[2*k] = ptr1;
						if(pos[a]<cnt->bbox[a]-1) nb[2*k+1] = ptr1+cnt->mul[a]; else nb[2*k+1] = ptr1;
					}
				Vecf<3> ff; accessor.conv<AID>(ptr1, (const char**)nb, &(ff[0]));
				float fres = dout==1? ff[0]: ff.abs();	      
#ifndef AIW_WIN32				
         f_min = std::min(f_min, fres);  f_max = std::max(f_max, fres);
#else //AIW_WIN32        
        if (act_numthreads < 0){
# pragma omp critical
            act_numthreads = omp_get_num_threads();
        }
        int me = omp_get_thread_num();
        f_mina[me] = std::min(f_mina[me], fres);  f_maxa[me] = std::max(f_maxa[me], fres);
#endif //AIW_WIN32	  
	  }// end of cells loop
#ifdef AIW_WIN32	  
      f_min = *std::min_element(f_mina.begin(), f_mina.begin()+ act_numthreads);
      f_max = *std::max_element(f_maxa.begin(), f_maxa.begin() + act_numthreads);
#endif //AIW_WIN32
			f_lim[0] = f_min;  f_lim[1] = f_max;  cnt->flimits[LID] = f_lim;
			accessor.minus = acc_minus;
#ifdef PROFILE
			fprintf(stderr, "limits %g sec\n", omp_get_wtime()-t0);
#endif //PROFILE
		} // конец расчета пределов
	}
	if(accessor.minus) f_lim = -f_lim(1,0);
	color.reinit(f_lim[0], f_lim[1]); 
}
//------------------------------------------------------------------------------
// template <int AID> void aiw::QpltMeshPlotter::get_impl(int xy[2], QpltGetValue &res) const   // принимает координаты в пикселях
template <int AID> void aiw::QpltMeshPlotter::get_impl(int xy[2], std::string &res) const {  // принимает координаты в пикселях
	Vecf<2> r; 
	for(int fID=0, sz = flats.size(); fID<sz; fID++) if(flats[fID].image2flat(xy[0], xy[1], r)){
		    auto f = get_flat<AID>(fID); Ind<2> pos; Vecf<2> X;
			f.mod_coord(r, pos, X); 
			// WERR(f.a[0], f.a[1], f.nX, f.nY, xy[0], xy[1], r);
			auto v = interpolate(f, pos, X, f.interp);
			std::stringstream S; S/*<<r<<':'*/<<v[0]; for(int i=1; i<v.size(); i++) S<<'\n'<<v[i];
			res = S.str(); break;
			// res.value = S.str(); for(int i=0; i<2; i++) res.xy[i] = container->fpos2coord(bbeg[f.axis[i]]+r[i], axisID[f.axis[i]]); 
			// f.flat2image(vecf(r[0], 0.f), res.a1);  f.flat2image(vecf(r[0], f.bbox[1]), res.a2);
			// f.flat2image(vecf(0.f, r[1]), res.b1);  f.flat2image(vecf(f.bbox[0], r[1]), res.b2);
		}
}
//------------------------------------------------------------------------------
template <int AID> void aiw::QpltMeshPlotter::plot_impl(std::string &res) const {
	// QpltColor c2D = color; c2D.reinit(0., 1.); QpltColor3D c3D; c2D.conf(&c3D);
	// for(int i=0; i<100; i++) printf("%06X %06X\n", 0xFFFFFFFF-QpltColor::rgb_t(c2D(i*1e-2)).inv().I, colorF2I(c3D(i*1e-2))); // std::cout<<c3D(i*1e-2)<<'\n'; 
	// exit(0);

#ifdef PROFILE
	double t0 = omp_get_wtime();
#endif //PROFILE
	
	constexpr int dout = QpltAccessor::DOUT<AID>();
	constexpr int PAID = dout==1? AID: (AID&0x7F)|(3<<6); // если итоговое поле векторное - рисуем его модуль
	QpltImage im(ibmax[0]-ibmin[0], ibmax[1]-ibmin[1]);

	if(dim==2 && flats.size()==1){ // 2D mode
		auto plt = get_flat<PAID>(0);
#pragma omp parallel for 
		for(int y=0; y<im.Ny; y++){
			Ind<2> pos; Vecf<2> X; plt.mod_coord2D(1, y, pos[1], X[1]);
			for(int x=0; x<im.Nx; x++){
				plt.mod_coord2D(0, x, pos[0], X[0]);
				im.set_pixel(x, y, color(interpolate(plt, pos, X, plt.interp)[0])); // ???
			}
		}
		//im.dump2ppm("/tmp/qplt.ppm");
		if(dout>1){  // векторные поля
			auto plt2 = get_flat<AID>(0);
			Ind<2> N = ind(im.Nx, im.Ny), Na = N/color.arr_length/color.arr_spacing<<plt2.bbox, d = N/Na;
			// WOUT(N, d, Na, plt2.bbox, plt2.diff3minus, plt2.diff3plus, bbeg[2]);
			for(int iy=0; iy<Na[1]; iy++)  for(int ix=0; ix<Na[0]; ix++){
					int x = ix*d[0]+d[0]/2,  y = iy*d[1]+d[1]/2; Ind<2> pos; Vecf<2> X;
					plt2.mod_coord2D(0, x, pos[0], X[0]);	plt2.mod_coord2D(1, y, pos[1], X[1]);
					// if(!y) WOUT(x, pos, X);
					auto v = interpolate(plt2, pos, X, plt.interp);
					color.arr_plot(x, y, (const float*)&v, im);
					// im.set_pixel(x, y, 0xFFFFFF);
				}
		}
	} else if(mode==1) { // pseudo 3D mode
		int fl_sz = flats.size(), cID = 0; QpltMeshFlat<PAID> calcs[4 /*fl_sz*/]; for(int i=0; i<fl_sz; i++) calcs[i] = get_flat<PAID>(i);
#pragma omp parallel for firstprivate(cID)
		for(int y=0; y<im.Ny; y++){
			int y0 = ibmin[1]+y;
			for(int x=0; x<im.Nx; x++){
				int x0 = ibmin[0]+x; Vecf<2> r;
				if(!calcs[cID].image2flat(x0, y0, r)){
					bool miss = true;
					for(int i=1; i<fl_sz; i++) if(calcs[(cID+i)%fl_sz].image2flat(x0, y0, r)){ miss = false; cID = (cID+i)%fl_sz; break; }
					if(miss){ im.set_pixel0(x, y, 0xFFFFFFFF); continue; }
				}
				Ind<2> pos; Vecf<2> X; calcs[cID].mod_coord(r, pos, X);
				im.set_pixel0(x, y, color(interpolate(calcs[cID], pos, X, calcs[cID].interp)[0]));
				// im.set_pixel0(x, y, 0xFF<<cID*8);
				
			}
		}
	} else { // real 3D mode
		QpltMeshPlotter3D<PAID> plt;
		plt.vtx.init(*this);  color.conf(&plt.color, D3tmask); plt.accessor = accessor; plt.D3mingrad = D3mingrad;
		plt.Nx = im.Nx; plt.Ny = im.Ny; plt.ibmin = ibmin; plt.bbox = bbox;
		int fl_sz = flats.size();  for(int i=0; i<fl_sz; i++) plt.flats[i] = get_flat<PAID>(i);
		for(int i=0; i<3; i++) plt.deltas[i] = icenter&(1<<(3-plt.flats[i].axis[0]-plt.flats[i].axis[1]))? -plt.flats[i].mul[2] : plt.flats[i].mul[2];
		plt._max_len = 1.f/(1+(bbox.min()-1)*(1-.01f*D3density)); plt.lim_w = .01*D3opacity;
		plt.cr_grad = fabs(color.get_max()-color.get_min())/(10*bbox.max())*pow(exp(log(10*bbox.max())*.01), D3mingrad);
		// plt.flat[i].acc надо поменять как то, но как? Вообще работать с копиями?

		// WERR(im.Nx, im.Ny, im_size, im.buf.size(), plt.Nx*plt.Ny*4);
		
		plt.plot((int*)(&im.buf[0]));

		WERR("OK");
	}
	// im.dump2ppm("1.ppm");
	std::swap(res, im.buf);
#ifdef PROFILE
	fprintf(stderr, "plot %g sec\n", omp_get_wtime()-t0);
#endif //PROFILE
}
//------------------------------------------------------------------------------

