# Copyright (C) 2017,2020-22 Antov V. Ivanov  <aiv.racs@gmail.com> with support Kintech Lab
# Licensed under the Apache License, Version 2.0

from math import *
import sys
#-------------------------------------------------------------------------------
def text_sz(text, paint, vertical=-1):
    rect = paint.boundingRect(0, 0, 1000, 1000, 0, text)
    return rect.height() if vertical==1 else rect.width() if vertical==0 else (rect.width(), rect.height())
#def text_sz3D(text, paint):
#    rect = paint.boundingRect(0, 0, 1000, 1000, 0, text)
#    return  rect.width(), rect.height() 
#-------------------------------------------------------------------------------
_add = lambda *args: (int(sum(x[0] for x in args)), int(sum(x[1] for x in args)))
_sub = lambda A, B: (A[0]-B[0], A[1]-B[1])
_and = lambda A, B: (A[0]*B[0], A[1]*B[1])
_mul = lambda A, B: A[0]*B[0]+A[1]*B[1]
_xmul = lambda A, x: (A[0]*x, A[1]*x)
_abs = lambda A: (A[0]**2+A[1]**2)**.5

def make_tics3D(limits, logscale, A, B, C, tic_len, paint, label='', is_z_label=False):  
    '''возвращает [(tic_pos, tic_text)], extend, [tics_lines...], label_pos
ось расположена на отрезке A--B, точка C --- центр изображения. Координаты A, B, С задаются в пикселях
tic_pos задаются как (x,y) с выравниванием по умолчанию. 
extend --- (d_left, d_top, d_right, d_bottom) - то насколько необходимо расширить изображение за счет тиков (все со знаком +)
tic_line --- кортежи (x1, y1, x2, y2), координаты отрезков
'''
    if limits[0]==limits[1]: print('bad limits for axe', label, limits, file=sys.stderr); limits = (limits[0]-1, limits[1]+1) 
    AB = _sub(B, A); lAB = _abs(AB)
    if not lAB: return [], [0]*4, [], None
    d = -AB[1]/lAB*tic_len, AB[0]/lAB*tic_len   
    if _mul(d, _sub(C, A))>0: d = -d[0], -d[1]    
    ticsL = calc_tics(limits[0], limits[1], logscale) # несколько вариантов расстановки тиков
    #if not ticsL: return [], (0,)*4, [], ((A[0]+B[0])/2, (A[1]+B[1])/2)  #<<< более точно возврщать позицию метки и extend
    textL = [num2strL(L, logscale) for L in ticsL]    # текстовые представления
    tszX = [sum(text_sz(t, paint, False) for t in L) for L in textL] # суммарные размеры тиков по X
    tszY = [sum(text_sz(t, paint, True)  for t in L) for L in textL] # суммарные размеры тиков по Y
    vertical = sum(tszX)/(abs(A[0]-B[0])+1) > sum(tszY)/(abs(A[1]-B[1])+1)
    tszs = tszY if vertical else tszX
    tics = sorted(ticsL[min([(abs(float(tszs[i])/lAB-[.4, .3][vertical]), i) for i in range(len(tszs))])[1]]) # набор значений тиков    
    if not tics: tics = limits
    if logscale:
        stics = [] if limits[0]==tics[0] else calc_tics_normalscale(limits[0], tics[0], tics[0]/10)
        for a, b in zip(tics[:-1], tics[1:]): stics += calc_tics_normalscale(a, b, 10**floor(log10(a)))
        stics += [] if limits[1]==tics[-1] else calc_tics_normalscale(tics[-1], limits[1], tics[-1])
    else: stics = calc_tics_normalscale(limits[0], limits[1], ((tics[1]-tics[0])/10 if len(tics)>1 else tics[0]))  # если в tics одно значение как расставлять subtics???
    t2p0 = lambda t: 1./log(limits[1]/limits[0])*log(t/limits[0]) if logscale else abs(1./(limits[1]-limits[0])*(t-limits[0])) # конвертер значения в позицию
    def t2p(t, sc=1): a = _add(A, _xmul(AB, t2p0(t))); return list(map(int, a+_add(a, _xmul(d, sc))))
    L, align, d2, extend, bbox, flow = num2strL(tics, logscale), [-(d[0]<0), -(d[1]<0)], _xmul(d, 2), [0]*4, [f(A[i], B[i]) for f in (min, max) for i in (0, 1)], None
    if abs(AB[0])>abs(AB[1]):  # ось скорее горизонтальная
        if abs(max(text_sz(l, paint, False) for l in L)/AB[0]*AB[1])<tic_len: flow, align[0] = 0, -.5  # выравнивание по центру
    elif abs(AB[0])<abs(AB[1]): flow, align[1] = 1, -.5  # ось скорее вертикальная, выравнивание по центру 
    T2p = lambda t, l: list(map(int, _add(A, _xmul(AB, t2p0(t)), d2, _and(align, text_sz(l)))))
    def T2pl(i):
        t, l = tics[i], L[i]
        if not flow is None:  # меняем align что бы чиселка не вылезала за пределы оси
            #align[flow] = -.5 #i/len(L)*-(d[flow]<0)
            #if i in (0, len(L)-1) and (t2p0(t)<1e-2 or t2p0(t)>.99): align[flow] = 0 if (A[flow]<B[flow])^bool(i)^flip else -1
            if i in (0, len(L)-1) and (t2p0(t)<1e-2 or t2p0(t)>.99): align[flow] = 0 if (A[flow]<B[flow])^bool(i) else -1
            else: align[flow] = -.5 #i/len(L)*-(d[flow]<0)
        lsz = text_sz(l, paint); lxy = list(map(int, _add(A, _xmul(AB, t2p0(t)), d2, _and(align, lsz))))
        update_extend(lxy, lsz)
        return lxy, l
    def update_extend(xy, sz):
        extend[0] = -min(xy[0]-bbox[0], -extend[0])
        extend[1] = -min(xy[1]-bbox[1], -extend[1])
        extend[2] = max(xy[0]+sz[0]-bbox[2], extend[2])
        extend[3] = max(xy[1]+sz[1]-bbox[3], extend[3])
    if label:
        lbl_sz, max_tic_sz = text_sz(label, paint), max(text_sz(l, paint, False) for l in L)
        if is_z_label: 
            if d[0]<0: lbl_pos = [A[0]-2*tic_len-max_tic_sz-2*lbl_sz[1], int((A[1]+B[1]+lbl_sz[0])/2)];  extend[0] = max(extend[0], bbox[0]-lbl_pos[0])
            else: lbl_pos = [A[0]+2*tic_len+max_tic_sz+lbl_sz[1], int((A[1]+B[1]+lbl_sz[0])/2)];  extend[2] = max(extend[2], lbl_pos[0]+lbl_sz[0]-bbox[0])
        else:
            mp = _add(_xmul(_add(A, B), .5), _xmul(d, (abs(_mul(d, (max_tic_sz, lbl_sz[1])))/tic_len+2*tic_len+lbl_sz[1])/tic_len)) # середина вынесенного от AB отрезка
            lbl_pos = _add(mp, _and(lbl_sz, align))
            update_extend(lbl_pos, lbl_sz)
    else: lbl_pos = None
    try: return list(map(T2pl, range(len(L)))),  extend, [t2p(t, 2) for t in tics]+list(map(t2p, stics)), lbl_pos
    except ValueError as e: return [], [0]*4, [], lbl_pos
#-------------------------------------------------------------------------------
def make_tics(limits, logscale, N, vertical, paint):
    'возвращает [(tic_pos, tic_text)... ], max_tic_sz, [subtics_pos...]'
    ticsL = calc_tics(limits[0], limits[1], logscale) # несколько вариантов расстановки тиков
    #tszs = [sum(text_sz(num2str(t), paint, vertical) for t in L) for L in ticsL] # суммарные размеры тиков
    tszs = [sum(text_sz(t, paint, vertical) for t in num2strL(L, logscale)) for L in ticsL] # суммарные размеры тиков
    tics = sorted(ticsL[min([(abs(float(tszs[i])/N-[.6, .4][vertical]), i) for i in range(len(tszs))])[1]]) # набор значений тиков    
    if not tics: tics = limits
    if logscale:
        stics = [] if limits[0]==tics[0] else calc_tics_normalscale(limits[0], tics[0], tics[0]/10)
        for a, b in zip(tics[:-1], tics[1:]): stics += calc_tics_normalscale(a, b, 10**floor(log10(a)))
        stics += [] if limits[1]==tics[-1] else calc_tics_normalscale(tics[-1], limits[1], tics[-1])
    else: stics = calc_tics_normalscale(limits[0], limits[1], (tics[1]-tics[0])/10)
    t2p = lambda t: int(N/log(limits[1]/limits[0])*log(t/limits[0]) if logscale else abs(N/(limits[1]-limits[0])*(t-limits[0]))) # конвертер значения в позицию
    L = num2strL(tics, logscale)        
    try: return [(t2p(t), l) for t, l in zip(tics, L)], max(text_sz(l, paint, False) for l in L), map(t2p, stics)
    except ValueError as e: return [(0, l) for l in L], max(text_sz(l, paint, False) for t in L), [] #[0]*len(stics)
#-------------------------------------------------------------------------------
def num2strL(L, logscale): 
    'единообразно приводит список чисел к самому лаконичному варианту записи в виде строк'
    L1, L2 = ['%g'%x for x in L], [] #; S1 = sum(map(len, L1))
    for x in L:
        try: p  = floor(log10(abs(x)))
        except: p = 0
        L2.append('%ge%g'%(x*10**-p, p))
    if logscale:  return [a if len(a)<=len(b) else b for a, b in zip(L1, L2)]
    L3 = [x.rstrip('0') for x in L1]
    LL = [len(a)-len(b) for a, b in zip(L1, L3)]; minLL = min(LL) if LL else 0
    if minLL>2: eX = 'e%i'%minLL; return [x+'0'*(l-minLL)+eX for x, l in zip(L3, LL)]  # единообразно заменяем нули на конце на eX
    #return [a if len(a)<=len(b) else b for a, b in zip(L1, L2)]
    return L1 if sum(map(len, L1))<=sum(map(len, L2)) else L2
#-------------------------------------------------------------------------------
def calc_tics_normalscale(a, b, h):
    'возвращает разбиение по шагам h'
    if not h: return []
    if b<a: a, b = b, a
    A, B, L = floor(a/h)*h, ceil(b/h)*h, []
    while A<=B:
        if a<=A<=b: L.append(0. if abs(A)<h/100 else A)
        A += h
    return L    
def calc_tics(a, b, logscale):
    'принимает границы диапазона и тип шкалы, возвращает несколько вариантов расстановки тиков парами'
    #if b<a: a, b = b, a
    if a==b: return []
    if logscale:
        A0, B, res = 10**floor(log10(a)), 10**ceil(log10(b)), []
        for n in range(1, 5):
            A, L = A0, []
            while A<=B:
                if a<=A<=b: L.append(A)
                A *= 10**n
            res.append(L)            
    else:
        H, res = 10**ceil(log10(abs(b-a))), []
        for h in [H, H/2, H/4, H/5, H/10, H/20, H/40, H/50, H/100, H/200, H/500, H/100]:
            L = calc_tics_normalscale(a, b, h)
            if len(L)<=20: res.append(L)
    return res
#-------------------------------------------------------------------------------
