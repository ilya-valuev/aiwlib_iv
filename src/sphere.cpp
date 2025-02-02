/**
 * Copyright (C) 2016, 2017-18, 2020 Sergey Khilkov <ezz666@gmail.com> and Antov V. Ivanov  <aiv.racs@gmail.com>
 * Licensed under the Apache License, Version 2.0
 **/

#include <map>
#include <algorithm>

#define INIT_ARR_ZERO  {nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr}
#include "../include/aiwlib/sphere"
using namespace aiw;

static const int MAX_RANK=12;
using ULInd2 = Vec<2,uint64_t>;
using ULInd3 = Vec<3,uint64_t>;
using ULInd6 = Vec<6,uint64_t>;
static int current_rank = -1;
//------------------------------------------------------------------------------
static Vec<3>* cell_centers[MAX_RANK] = INIT_ARR_ZERO;    // центры ячеек
static double* cell_areas[MAX_RANK] = INIT_ARR_ZERO;      // площади ячеек
static ULInd3* cell_vertex[MAX_RANK] = INIT_ARR_ZERO;       // индексы вершин ячейки
static Vec<3>* vertex = nullptr;                          // координаты вершин
static ULInd3* cell_neighbours[MAX_RANK] = INIT_ARR_ZERO;   // индексы соседних ячеек (для ячейки)
static ULInd6* vertex_cells[MAX_RANK] = INIT_ARR_ZERO;      // индексы ячеек (для вершины)
static Vec<3>* normals[MAX_RANK] = INIT_ARR_ZERO;         // нормали (хранятся тройками?)

static ULInd3* cell_edges[MAX_RANK] = INIT_ARR_ZERO;        // индексы ребер ячейки (в оппозит вершинам)
static ULInd2* edge_cells[MAX_RANK] = INIT_ARR_ZERO;        // индексы ячеeк ребра
static ULInd2* edge_vertex[MAX_RANK] = INIT_ARR_ZERO;       // индексы вершин ребра
static ULInd6* vertex_vertex[MAX_RANK] = INIT_ARR_ZERO;     // индексы соседних вершин (для вершины)
static ULInd6* vertex_edges[MAX_RANK] = INIT_ARR_ZERO;      // индексы соседних ребер (для вершины)
static double* vertex_areas[MAX_RANK] = INIT_ARR_ZERO;    // площади ячеек при разбиении по вершинам 
static double* edge_areas[MAX_RANK] = INIT_ARR_ZERO;      // площади ячеек при разбиении по ребрам 
static Vec<3>* edge_centers[MAX_RANK] = INIT_ARR_ZERO;    // координаты центров ребер

static double* interp_radius[MAX_RANK] = INIT_ARR_ZERO;   // радиусы носителей при интерполяции с mode=1
//------------------------------------------------------------------------------
Vec<3> aiw::barecentr(const Vec<3> &n, const Vec<3> tr[3]){
    Vec<3> l;
    for (int i=0; i<3; i++) l[i] = (n%tr[(i+1)%3])*tr[(i+2)%3];
    return l/(l[0]+l[1]+l[2]);
}
//------------------------------------------------------------------------------
template<int N,typename T> inline Vec<N,T> shift( const Vec<N,T>& r, int i ) {
	Vec<N,T> result;
	if (i>=N || i<=-N) i = i%N;
	if (i<0) i+=N;
	for(int j=i; j<N; j++) result[j-i]=r[j];
	for(int j=0; j<i; j++) result[N-i+j]=r[j];
	//WOUT(r, i, result)
	return result;
}
//------------------------------------------------------------------------------
//вспомогательная ф-я определяющая номер грани
uint64_t id( const Vec<5,uint64_t>& ind ) { return ( ( ind[ (ind[3]+1)%3 ]+1 )/2 + ind [ (ind[3]+2)%3 ]+1 + 4*ind[3] )*5 + ind[4]; }
//------------------------------------------------------------------------------
//вспомагательная функция возвращающая соседний снизу треугольник для сетки 0-го уровня.
inline Vec<5, uint64_t> down_nb(Vec<5, uint64_t> ind){
	ind[ind[3]]=1;
	int l = ind[0] * ind[1] * ind[2];//Это не ind.volume()
	ind[ ind[3] ] = ind[ ind[3] ]*(1-l)*0.5-(l+1)*0.5;
	Vec<5,int64_t> nb;
	if ( ind[4]<2 ) {
		Vec<3,int64_t> tmp = ULInd3(ind[0],ind[1],ind[2]);
		tmp[ind[3]] = -tmp[ind[3]];
		nb =  tmp | (ind[3]+1+ind[4])%3 | (5-ind[4]+1)%5;
	} else {
		if ( ind[4]==4 ) {
			Vec<3,int64_t> tmp = ULInd3(ind[0],ind[1],ind[2]);
			tmp[(ind[3]+1)%3] = -tmp[(ind[3]+1)%3];
			nb = tmp|ind[3]|ind[4];
		} else {
			Vec<3,int64_t> tmp = ULInd3(ind[0],ind[1],ind[2]);
			nb = tmp|(ind[3]+4-ind[4])%3|(5-ind[4]);
		}
	}
	return nb;
}
//------------------------------------------------------------------------------
inline Vec<3> refr( Vec<3> r, int i ) { int p = i%3; r[p] =- r[p]; return r; }  //Отражение от i-й(mod 3) координатной плоскости
//------------------------------------------------------------------------------
int aiw::sph_max_rank(){  // максимальный инициализированный ранг
	return current_rank; // Потенциальный race
}
//------------------------------------------------------------------------------
size_t aiw::sph_cells_num(int rank){
	if(rank>=0 && rank<30) return 60l<<2*rank; //30 не влезает в uint64_t
	else return 0;
}
size_t aiw::sph_vertex_num(int rank){
	if(rank>=0 && rank<30) return (30l<<2*rank) +2l; //30 не влезает в uint64_t
	else return 0;
}
size_t aiw::sph_edges_num(int rank){ 
	if(rank>=0 && rank<30) return 90l<<2*rank; //30 не влезает в uint64_t
	else return 0;
}
//------------------------------------------------------------------------------
// возвращает число точек, максимум 13? Для ответа надо свернуть IDs*weights
int aiw::sph_interp_weights(const aiw::Vec<3> &r, int R, int mode, uint64_t* IDs, double *weights){ 
	size_t cid = sph_cellInd(r, R); // ячейка сетки куда попал r
	const aiw::Vec<3, uint64_t>& vid = sph_cell_vert(cid, R); // ID вершин ячейки сетки куда попал r
	Vec<3> n[3]; for(int i=0; i<3; i++) n[i] = sph_vert(vid[i], R); // координаты вершин ячейки сетки куда попал r
	if(mode){ // по граням не интреполируем?
		/* linear interp
		Vec<3> w = barecentr(r, n); // веса вершин ячейки сетки куда попал r
		for(int i=0; i<3; i++){ IDs[i] = vid[i]; weights[i] = w[i]; }
		return 3;
		*/
		double c_max = 0; uint64_t v_max = -1; int sz = 0;
		for(auto i: vid){ double c = sph_vert(i, R)*r; if(c_max<c){ c_max = c; v_max = i; } }
		const auto &vIDs = sph_vert_vert(v_max, R);
		IDs[sz] = v_max; weights[sz++] = r*sph_vert(v_max, R);
		for(auto i: vIDs) if(i!=uint64_t(-1)){ double c = r*sph_vert(i, R); if(c>=interp_radius[R][i]){ IDs[sz] = i; weights[sz++] = c; } }
		double w_sum = 0; for(int i=0; i<sz; i++){ weights[i] -= interp_radius[R][IDs[i]]; weights[i] *= weights[i]; w_sum += weights[i]; }
		w_sum = 1./w_sum; for(int i=0; i<sz; i++) weights[i] *= w_sum;
		return sz;
	}
	Vec<3> cXn[3], c = sph_cell(cid, R); for(int i=0; i<3; i++) cXn[i] = c%n[i];  // центр ячейки и векторные произведения центра с вершинами
	int sz = 0, tr;  // число ячеек и номер треугольника (дальней от r вершины)?
	for(tr=0; tr<3; tr++) if((cXn[(tr+1)%3]*r)*(cXn[(tr+1)%3]*n[(tr+2)%3])>=0 && (cXn[(tr+2)%3]*r)*(cXn[(tr+2)%3]*n[(tr+1)%3])>=0) break;
	if(tr==3) WRAISE("oops...", r, cid, vid, n, cXn, c);
	n[tr] = c; Vec<3> w = barecentr(r, n); // веса вершин 1/3 ячейки сетки куда попал r
	IDs[sz] = cid; weights[sz++] = w[tr];
	for(int i=0; i<3; i++){
		if(i==tr) continue;
		const aiw::Vec<6, uint64_t>& nbid = sph_vert_cell(vid[i], R); // ячейки, к которым относится вершина
		int m = 6 - (nbid[5]==uint64_t(-1)); // число соседних ячеек
		for(int j=0; j<m; j++){ IDs[sz] = nbid[j]; weights[sz++] = w[i]/m; }
	}
	return sz; 
}
//------------------------------------------------------------------------------
void aiw::sph_free_table(int rank){ // освобождает таблицы старше ранга rank (включительно)
	int old_rank = current_rank; current_rank = rank;
	for(int i=old_rank; i<=rank; i-- ){
		delete [] cell_centers[i]; cell_centers[i] = nullptr;
		delete [] cell_areas[i];   cell_areas[i] = nullptr;
		delete [] cell_vertex[i];  cell_vertex[i] = nullptr;
		delete [] cell_neighbours[i]; cell_neighbours[i] = nullptr;
		delete [] vertex_cells[i]; vertex_cells[i] = nullptr;
		delete [] normals[i];  normals[i] = nullptr;
		delete [] cell_edges[i];  cell_edges[i] = nullptr;
		delete [] edge_cells[i];  edge_cells[i] = nullptr;
		delete [] edge_vertex[i];  edge_vertex[i] = nullptr;
		delete [] vertex_vertex[i];  vertex_vertex[i] = nullptr;
		delete [] vertex_edges[i];  vertex_edges[i] = nullptr;
		delete [] vertex_areas[i];  vertex_areas[i] = nullptr;
		delete [] edge_areas[i];  edge_areas[i] = nullptr;
		delete [] edge_centers[i];  edge_centers[i] = nullptr;
		delete [] interp_radius[i];  interp_radius[i] = nullptr;
	}
	Vec<3> *tmp3 = vertex;
	vertex = new Vec<3>[sph_vertex_num(current_rank)];
	for(uint64_t i=0; i< sph_vertex_num(current_rank); i++) vertex[i] = tmp3[i];
	delete [] tmp3;
}
//------------------------------------------------------------------------------
void init_zero_rank(){
	// здесь должна быть инициализация массивов для 0-го уровня рекурсии.
	// Заполнение vertex, vertex_cells, cell_vertex
	// Здесь два типа вершин - вершины додекаэдра и центры граней,
	// их необходимо обрабатывать отдельно, примерно так, как это сделано в  incid
	// параллельно нужно будет проставить им номера, и задать их координаты
	//  вместе с этим задаются вершины соответствующие одной ячейке.
	const double _edge_arg = 9.-8.*cos(0.6*M_PI)+sqrt(13.-16.*cos(0.6*M_PI)); //длина ребра додекаэдра
	const double edge = 2.*sqrt(2./_edge_arg); //длина ребра додекаэдра
	const Vec<3> vect0[2] = {Vec<3>( .5*edge, 0, sqrt( 1. - .25*edge*edge ) ),
							 Vec<3>(1./ sqrt(3) )}; //2 вершины додеккаэдра додекаэдра
	cell_vertex[0] = new ULInd3[60];
	vertex_cells[0] = new ULInd6[32];
	cell_neighbours[0] = new ULInd3[60];
	//if (!vertex) vertex = new Vec<3>[32];//impossible
	int ver = 0;
	bool* tmp = new bool[60];
	cell_centers[0] = new Vec<3>[72];
	for (int i = 0; i < 60; i++) tmp[i] = false;
	for (int i=0; i<12; i++) {
		Vec<3, int> ind(1,1,1);
		int ind3 = i>>2;
		ind[ (ind3+1)%3 ] = (i%2)*2 - 1;
		ind[ (ind3+2)%3 ] = (i%4) - ( ( ind[ (ind3+1)%3 ] + 1 ) >> 1 ) - 1;
		int l = ind[0]*ind[1]*ind[2], j = (l<0) ? (ind3+2)%3 : (3 - ind3)%3;
		//пятиугольник по часовой стрелке.
		Vec<3> pentag[5];
		pentag[ (5-l)%5 ] = shift( vect0[0], (3+l*j)%3 )&ind;
		pentag[ (6-l)%5 ] = vect0[1]&ind;
		pentag[ (7-l)%5 ] = shift( vect0[0], (3+l*(j+1))%3 )&ind;
		pentag[ (8-l)%5 ] = refr( pentag[ (2 - (l+1)/2 - l)%5 ], ind3 );
		pentag[ (9-l)%5 ] = refr( pentag[ (6 - (l+1)/2 - l)%5 ], ind3 );
		Vec<3> cent(0.);
		//vctr<3> x  = pentag[0];
		for(int k=0; k<5; k++){
			if ( !tmp[5*i+k] ) {
				//Задание треугольников инцидентных вершине.
				ULInd6 buf;
				buf[0] = 5*i+k;
				buf[5] = 5*i+((k+1)%5);
				Vec<5,uint64_t> tmp2 = down_nb(ind|ind3|((k+1)%5));
				buf[4] = id(tmp2);
				tmp2[4] = (tmp2[4]+1)%5;
				buf[3] = id(tmp2);
				tmp2 = down_nb(tmp2);
				buf[2] =  id(tmp2);
				tmp2[4] = (tmp2[4]+1)%5;
				buf[1] = id(tmp2);
				vertex_cells[0][ver] = buf;
				//Заполнение массива вершин
				vertex[ver] = pentag[k];
				ver++;
				//Исключение дублирования вершин
				tmp[buf[0]] = 1;
				tmp[buf[2]] = 1;
				tmp[buf[4]] = 1;
				for(int p=0; p<6;p++){
					cell_vertex[0][ buf[p] ][ (p%2)*2 ] = ver-1;
				}
			}
			cent += pentag[k];
		}
		vertex[ver]  = cent/cent.abs();
		cell_centers[0][60+i] = vertex[ver];
		vertex_cells[0][ver] = ULInd6( 5*i, 5*i+1,5*i+2, 5*i+3, 5*i+4, -1);
		for(int p=0; p<5;p++){
			cell_vertex[0][ vertex_cells[0][ver][p] ][1] = ver;
		}
		ver++;
	}
	if (tmp) {delete [] tmp;tmp=0;}
	for(int i =0 ; i<32;i++){
		ULInd6 cur = vertex_cells[0][i];
		if (cur[5]!=uint64_t(-1)){
			for(int j=0; j<6;j++){
				cell_neighbours[0][cur[j]][((j%2)*2+1)%3] = cur[(j+1)%6];//Здесь тоже есть лажа, но этого не видно, может её и нет?
				cell_neighbours[0][cur[j]][((j%2)*2+2)%3] = cur[(j+5)%6];
			}
		} else {
			for(int j=0; j<5; j++){
				cell_neighbours[0][cur[j]][2] = cur[(j+1)%5];
				cell_neighbours[0][cur[j]][0] = cur[(j+4)%5];
			}
		}
	}
}
//------------------------------------------------------------------------------
void mass_finish(int rank){
	uint64_t CurN = sph_cells_num(rank);
	uint64_t CurVN = sph_vertex_num(rank);
	uint64_t PrevN= sph_cells_num(rank-1);
	uint64_t PrevVN= sph_vertex_num(rank-1);
	//WOUT(rank);
	if (rank!=0){
		if(!normals[rank]){
			normals[rank] = new Vec<3>[3 *  PrevN];
			//нормали
			for(uint64_t k = 0; k <PrevN ; k ++){
				Vec<3>  
					r0 = vertex[ cell_vertex[rank][4*k][0] ],
					r1 = vertex[ cell_vertex[rank][4*k][1] ], 
					r2 = vertex[ cell_vertex[rank][4*k][2] ];
				normals[rank][ 3*k ] = r2%r1;
				normals[rank][ 3*k+1 ] = r0%r2;
				normals[rank][ 3*k+2 ] = r1%r0;
			}
		}
	}
	if(!cell_areas[rank]) cell_areas[rank] = new double[CurN];
	
	if(!cell_centers[rank]) cell_centers[rank] =  new Vec<3>[CurN];
	
	//площади и центры
	for(uint64_t k=0; k<CurN ; k++){
		Vec<3>  
			r0 = vertex[ cell_vertex[rank][k][0] ],
			r1 = vertex[ cell_vertex[rank][k][1] ], 
			r2 = vertex[ cell_vertex[rank][k][2] ];
		cell_areas[rank][k] = ((r0-r1)%(r0-r2)*0.5).abs();
		cell_centers[rank][k] = (r0+r1+r2)/(r0+r1+r2).abs();
	}
	//соседи
	if(!cell_neighbours[rank]){
		cell_neighbours[rank] = new ULInd3[CurN];
		for(int k =0; k< 32; k++){
			ULInd6 cur  = vertex_cells[rank][k];
			if(cur[5] != uint64_t(-1)) {
				for(int i=0; i<6; i++){
					int num = (i%2)*2;
					cell_neighbours[rank][cur[i]][(num+2)%3] = cur[(i+5)%6];
					cell_neighbours[rank][cur[i]][(num+1)%3] = cur[(i+1)%6];
				}
			} else {
				//                int num = 1;
				for(int i=0; i<5; i++){
					cell_neighbours[rank][cur[i]][0] = cur[(i+4)%5];
					cell_neighbours[rank][cur[i]][2] = cur[(i+1)%5];
				}
			}
		}
		for(uint64_t k =32; k< PrevVN; k++){
			ULInd6 cur  = vertex_cells[rank][k];
			for(int i =0; i<6;i++){
				int num = cur[i]%4 - 1;//0 быть не должно
				//нужно узнать номер вершины для удобного задания порядка
				cell_neighbours[rank][cur[i]][(num+2)%3] = cur[(i+5)%6];
				cell_neighbours[rank][cur[i]][(num+1)%3] = cur[(i+1)%6];
			}
		}
		for(uint64_t k = PrevVN; k<CurVN; k++){
			ULInd6 cur = vertex_cells[rank][k];
			int num[2] = {int(5 - cur[0]%4 - cur[2]%4), int(5 - cur[3]%4 - cur[5]%4)}; // приведение типов???
			ULInd3 vni[3] = {
				ULInd3(1,0,2),
				ULInd3(2,1,0),
				ULInd3(0,2,1)
			};// номера вершин в соответствующих треугольниках
			for(int i=0; i<3; i++){
				cell_neighbours[rank][cur[i]][(vni[num[0]][i]+1)%3] = cur[(i+1)%6];
				cell_neighbours[rank][cur[i]][(vni[num[0]][i]+2)%3] = cur[(i+5)%6];
				cell_neighbours[rank][cur[i+3]][(vni[num[1]][i]+1)%3] = cur[(i+4)%6];
				cell_neighbours[rank][cur[i+3]][(vni[num[1]][i]+2)%3] = cur[(i+2)%6];
			}
		}
	}
}
//------------------------------------------------------------------------------
void arrs_init(int rank){
	//WOUT(rank);
	if(rank<0 || rank>=MAX_RANK) WRAISE("incorrect ", rank, MAX_RANK);
	if(rank==0) init_zero_rank();
	else {
		//тут нужна другая функция
		//Увеличивать быстродействе здесь будем потом
		//Заполнение vertex vertex_cells  cell_vertex
		uint64_t CurN = sph_cells_num(rank);
		uint64_t PrevN = CurN/4;//Для читаемости.
		uint64_t CurVN = sph_vertex_num(rank);
		uint64_t PrevVN = sph_vertex_num(rank-1);
		cell_vertex[rank] = new ULInd3[CurN];
		vertex_cells[rank] = new ULInd6[CurVN];
		bool* tmp = new bool[PrevN*3];//т.к. в нашем обходе вершины могут (и будут) встречаться дважды, мы будем проверять, что они ещё не пройдены
		for (uint64_t i = 0; i < PrevN*3; i++) tmp[i] = 0;
		ULInd3  cni[3] = {
			ULInd3(3,0,2),
			ULInd3(1,0,3),
			ULInd3(2,0,1)
		};//треугольники при вершине со стороны 0 ,1 или 2
		ULInd3  vni[3] = {
			ULInd3(1,0,2),
			ULInd3(2,1,0),
			ULInd3(0,2,1)
		};// номера вершин в соответствующих треугольниках
		int exch[3] = {2, 1, 0};//перестановка (2,0)
		uint64_t ver = PrevVN;
		for ( uint64_t i = 0; i < PrevN; i++ ){//цикл по всем центральным треугольникам нашей сетки
			for (int num = 0; num < 3; num++ ){//цикл по номеру вершины центрального треугольника
				if ( (!tmp[ 3*i + num ])){//&& (ver< k) ){
					// необходимость в дополнительнм условии пропала
					uint64_t nb  = cell_neighbours[rank-1][i][num];
					//                    if (rank==8) WOUT(i, nb, num);
					bool orient = (cell_vertex[rank-1][i][1]==cell_vertex[rank-1][nb][1]);
					vertex[ver] = vertex[ cell_vertex[rank-1][i][(num+1)%3] ] + vertex[cell_vertex[rank-1][i][(num+2)%3]];
					vertex[ver]*=1/vertex[ver].abs();
					ULInd3 ind(i*4);
					ULInd3 inb(nb*4);
					vertex_cells[rank][ver] = (ind+cni[num])|(inb+cni[ exch[num]*orient +(1-orient)*num ]);//ещё пройтись по всем из vertex_cells  и записать в cell_vertex
					for(int l = 0; l < 3; l++){
						cell_vertex[rank][ vertex_cells[rank][ver][l] ] [ vni[num][l] ] = ver;
						cell_vertex[rank][ vertex_cells[rank][ver][l+3] ] [  vni[exch[num]*orient +(1-orient)*num ][l] ] = ver;
					}
					tmp[3*nb+orient*exch[num] + (1-orient)*num] = 1;
					tmp[3*i+num]=1;
					ver++;
				}
			}
		}
		if ( tmp ){ delete [] tmp; tmp=0;}
		//для предыдущих уровней распространить vertex_cells  на текущий и дописать cell_vertex
		if (rank>1) {
			// uint64_t Prev2N = PrevN/4;
			uint64_t Prev2VN = sph_vertex_num(rank-2);
			for(uint64_t i = 0; i< Prev2VN; i++ ){
				ULInd6 cur  = vertex_cells[rank-1][i];
				ULInd6 next = ULInd6(cur[0]%4,cur[1]%4,cur[2]%4, cur[3]%4, cur[4]%4, cur[5]>0?cur[5]%4:3 );
				vertex_cells[rank][i] = cur * 4 + next;
				for(int l = 0; l <(cur[5]==uint64_t(-1)?5:6); l++){
					cell_vertex[rank][ vertex_cells[rank][i][l] ] [ next[l]-1 ] = i;
				}//next[l] не может быть 0, т.к. 0 инцидентны только вершинам появившимся на текущем уровне
			}
			for(uint64_t i = Prev2VN; i < PrevVN; i++ ){//Вершины появившиеся на предыдущем уровне рекурсии
				ULInd6 cur =vertex_cells[rank-1][i];
				int num1 = 5 - (cur[0]%4) -(cur[2]%4), num2 = 5 - (cur[3]%4) - (cur[5]%4);
				ULInd6 next = (vni[num1]|vni[num2])+ ULInd6((uint64_t)1);
				vertex_cells[rank][i] = cur * 4 +next;
				for(int l =0 ; l<6; l++){
					cell_vertex[rank][ vertex_cells[rank][i][l] ][next[l] -1] = i;
				}//среди этих вершин заведомо нет центров граней
			}
		} else {
			for(int i=0; i< 32; i++){
				ULInd6 cur = vertex_cells[0][i];
				ULInd6 next = (cur[5]!=uint64_t(-1))? ULInd6(1,3,1,3,1,3):ULInd6(2,2,2,2,2,3);
				vertex_cells[rank][i] = cur*4+next;
				for(int l = 0; l < (cur[5]==uint64_t(-1)?5:6); l++){
					cell_vertex[rank][ vertex_cells[rank][i][l] ] [ next[l] -1 ] = i;
				}
			}
		}
	}
	mass_finish(rank);
	
	// добиваем вершины и грани
	uint64_t cells_sz = sph_cells_num(rank), vertex_sz = sph_vertex_num(rank), edges_sz = sph_edges_num(rank);
	cell_edges[rank] = new ULInd3[cells_sz];       // индексы ребер ячейки (в оппозит вершинам)
	edge_cells[rank] = new ULInd2[edges_sz];       // индексы ячеeк ребра
	edge_vertex[rank] = new ULInd2[edges_sz];      // индексы вершин ребра
	vertex_vertex[rank] = new ULInd6[vertex_sz];   // индексы соседних вершин (для вершины)
	vertex_edges[rank] = new ULInd6[vertex_sz];    // индексы соседних ребер (для вершины)
	vertex_areas[rank] = new double[vertex_sz];  // площади ячеек при разбиении по вершинам 
	edge_areas[rank] = new double[edges_sz];     // площади ячеек при разбиении по ребрам 
	edge_centers[rank] = new Vec<3>[edges_sz];   // координаты центров ребер

	for(size_t i=0; i<vertex_sz; i++){
		vertex_vertex[rank][i] = vertex_edges[rank][i] = ULInd6(uint64_t(-1));
		vertex_areas[rank][i] = 0.;
	}
	
	std::map<ULInd2, size_t> edges_table; // таблица пары ячеек: - ID ребер
	for(size_t i=0; i<cells_sz; i++){
		const Vec<3> &c0 = cell_centers[rank][i];
		ULInd3& cids = cell_neighbours[rank][i]; // ID ячеек
		ULInd3& vids = cell_vertex[rank][i];     // ID вершин
		for(int k=0; k<3; k++){ 
			ULInd2 ec = ULInd2(i, cids[k]).sort(), ev(vids[(k+1)%3], vids[(k+2)%3]);
			size_t eID = edges_table.size(); auto I = edges_table.find(ec);
			if(I==edges_table.end()){
				edges_table[ec] = eID; edge_cells[rank][eID] = ec; 
				edge_vertex[rank][eID] = ev;
				for(int kk=0; kk<2; kk++) for(int j=0; j<6; j++) if(vertex_edges[rank][ev[kk]][j]==uint64_t(-1)){
							vertex_edges[rank][ev[kk]][j] = eID; vertex_vertex[rank][ev[kk]][j] = ev[1-kk]; break;
						}
				Vec<3> &er = edge_centers[rank][eID]; er = vertex[ev[0]]+vertex[ev[1]]; er /= er.abs();
				const Vec<3> &c1 = cell_centers[rank][cids[k]], &v0 = vertex[ev[0]], &v1 = vertex[ev[1]];
				double trS[4] = {((c0-er)%(v0-er)).abs(), ((c1-er)%(v0-er)).abs(), ((c0-er)%(v1-er)).abs(), ((c1-er)%(v1-er)).abs()};
				edge_areas[rank][eID] = .5*(trS[0]+trS[1]+trS[2]+trS[3]);
				vertex_areas[rank][ev[0]] += .5*(trS[0]+trS[1]);
				vertex_areas[rank][ev[1]] += .5*(trS[2]+trS[3]);
			} else eID = I->second;
			cell_edges[rank][i][k] = eID;			
		}
	}
	//--------------------
	size_t sz = sph_vertex_num(rank);
	interp_radius[rank] = new double[sz];
	for(size_t i=0; i<sz; i++){
		const Vec<3> &n0 = sph_vert(i, rank); double r_min = 1;
		const auto &cIDs = sph_vert_cell(i, rank);
		for(auto j: cIDs) if(j!=uint64_t(-1)){
				const auto &nIDs = sph_cell_cell(j, rank);
				for(auto k: nIDs){ double r = sph_cell(k, rank)*n0; if(r_min>r) r_min = r; }
			}
		interp_radius[rank][i] = r_min;
	}
}
//------------------------------------------------------------------------------
void aiw::sph_init_table(int rank){
	//  WOUT(_R, AR);
	//WOUT(rank, current_rank);
	if(current_rank>=rank) return;
	else {
		Vec<3> *tmp3 = vertex;
		vertex = new Vec<3>[sph_vertex_num(rank)];
		for(uint64_t i=0; i<sph_vertex_num(current_rank); i++) vertex[i] = tmp3[i];
		if (tmp3) delete [] tmp3;
		//WOUT(rank);
		for(int i=std::max(current_rank+1,0); i<=rank; i++){
			arrs_init(i); // инициализация массивов
		}
	}
	current_rank = rank;
	//  WOUT(2);
}
//------------------------------------------------------------------------------
size_t aiw::sph_cellInd(const Vec<3> &r, int rank){ // пока только для существующего ранга
	WASSERT(r*r>0, "incorrect direction ", r);
	double max_a = 0., a ; uint64_t id=-1;
	for( int i=60; i<72; i ++ ){
		//      WOUT(i, r, cell_centers );
		a = r*cell_centers[0][i];
		if( a>max_a ){  max_a = a; id=i; }
		}
	max_a = 0.;
	int st = (id-60)*5, fin = (id-59)*5;
	for( int i=st; i<fin; i++ ){
		//      WOUT(i);
		a = r*cell_centers[0][i];
		if( a>max_a ){ max_a = a; id=i; }
		}
	for( int R=1; R<=std::min(current_rank, rank); R++ ){
		//      WOUT(rank);
		Vec<3>* n = normals[R] + id*3;
		id = id*4 + ( n[0]*r>0 ) + 2*( n[1]*r>0 ) + 3*( n[2]*r>0 );//Нормали внешние
	}//сюда можно добавить корректировку
	return id;
}
//------------------------------------------------------------------------------
size_t aiw::sph_vertInd(const Vec<3> & r, int rank){ // пока только для существующего ранга
	const aiw::Vec<3, uint64_t>& vIDs = sph_cell_vert(sph_cellInd(r, rank), rank);
	Vec<3> p; for(int i=0; i<3; i++) p[i] = sph_vert(vIDs[i], rank)*r;
	return vIDs[p.imax()];
}
//------------------------------------------------------------------------------
size_t aiw::sph_edgeInd(const Vec<3> & r, int rank){ // пока только для существующего ранга
	const aiw::Vec<3, uint64_t>& eIDs = sph_cell_edge(sph_cellInd(r, rank), rank);
	Vec<3> p; for(int i=0; i<3; i++) p[i] = sph_edge(eIDs[i], rank)*r;
	return eIDs[p.imax()];
}
//------------------------------------------------------------------------------
const Vec<3>& aiw::sph_cell(size_t ID, int rank){ // центр ячейки
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return cell_centers[rank][ID];
}
//------------------------------------------------------------------------------
double aiw::sph_cell_area(size_t ID, int rank){ // площадь ячейки
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return cell_areas[rank][ID];
}
//------------------------------------------------------------------------------
const ULInd3& aiw::sph_cell_vert(size_t ID, int rank){ // индексы вершин ячейки
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return cell_vertex[rank][ID];
}
//------------------------------------------------------------------------------
const ULInd3& aiw::sph_cell_cell(size_t ID, int rank){ // близжайшие соседи ячейки
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return cell_neighbours[rank][ID];
}
// const aiw::Vec<3, uint64_t>& sph_cell_edge(uint64_t ID, int rank); // близжайшие ребра ячейки
//------------------------------------------------------------------------------
const Vec<3>& aiw::sph_vert(size_t ID, int rank){ // вершина (узел) сетки
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return vertex[ID];
}
//------------------------------------------------------------------------------
const ULInd6& aiw::sph_vert_cell(size_t ID, int rank){ // ячейки, к которым относится вершина
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return vertex_cells[rank][ID];
}
//------------------------------------------------------------------------------
const ULInd3& aiw::sph_cell_edge(size_t ID, int rank){ // индексы ребер ячейки (в оппозит вершинам)
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return cell_edges[rank][ID];
}
//------------------------------------------------------------------------------
const ULInd2& aiw::sph_edge_cell(size_t ID, int rank){ // индексы ячеeк ребра
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return edge_cells[rank][ID];
}
//------------------------------------------------------------------------------
const ULInd2& aiw::sph_edge_vert(size_t ID, int rank){  // индексы вершин ребра
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return edge_vertex[rank][ID];
}
//------------------------------------------------------------------------------
const ULInd6& aiw::sph_vert_vert(size_t ID, int rank){ // индексы соседних вершин (для вершины)
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return vertex_vertex[rank][ID];
}
//------------------------------------------------------------------------------
const ULInd6& aiw::sph_vert_edge(size_t ID, int rank){  // индексы соседних ребер (для вершины)
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return vertex_edges[rank][ID];
}
//------------------------------------------------------------------------------
double aiw::sph_vert_area(size_t ID, int rank){  // площади ячеек при разбиении по вершинам
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return vertex_areas[rank][ID];
}
//------------------------------------------------------------------------------
double aiw::sph_edge_area(size_t ID, int rank){  // площади ячеек при разбиении по ребрам
	WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
	return edge_areas[rank][ID];
}
//------------------------------------------------------------------------------
const Vec<3>& aiw::sph_edge(size_t ID, int rank){ // координаты центров ребер
    WASSERT(0<=rank && rank<=current_rank, "illegal rank: ", rank, current_rank); // ЗАГЛУШКА
    return edge_centers[rank][ID];
}
//------------------------------------------------------------------------------
