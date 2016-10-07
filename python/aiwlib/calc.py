# -*- coding: utf-8 -*-
import os, sys, time, cPickle, socket 
import aiwlib.mixt as mixt 
import aiwlib.chrono as chrono
#-------------------------------------------------------------------------------
_is_swig_obj = lambda X: all([hasattr(X, a) for a in ('this', 'thisown', '__swig_getmethods__', '__swig_setmethods__')])
_rtable, _G, ghelp = [], {}, []
_ignore_list = 'path statelist runtime progress args _progressbar md5sum'.split()
_racs_params, _racs_cl_params, _cl_args, _args_from_racs, _arg_seqs, _arg_order = {}, set(), [], [], {}, []
#-------------------------------------------------------------------------------
def _init_hook(self): pass
def _make_path_hook(self): 
    self.path = mixt.make_path(_racs_params['_repo']%self, _racs_params['_calc_num']) 
    return self.path
#-------------------------------------------------------------------------------
class Calc:
    '''Работа с расчетом (записью в базе) --- создание уникальной директории расчета 
    и сохранение/восстановление параметров в файле .RACS'''
    _starttime = time.time() #???
    #---------------------------------------------------------------------------
    def __init__(self, **D):
        self._starttime, self.runtime, self.statelist = time.time(), chrono.Time(0.), []
        self.progress, self.args, self._wraps = 0., list(_cl_args), []
        for k, v in D.items(): # обработка аргументов конструктора
            if k in _racs_params and not k in _racs_cl_params: _racs_params[k] = v
            elif not k in _racs_params: self.__dict__[k] = v
        #-----------------------------------------------------------------------
        #   серийный запуск и демонизация расчета
        #-----------------------------------------------------------------------
        if _arg_seqs:
            global _args_from_racs; _base_args_from_racs = list(_args_from_racs)
            copies, pids = _racs_params['_copies'], []
            queue = reduce(lambda L, a: [l+[(a,x)] for x in _arg_seqs[a] for l in L], _arg_order, [[('racs_master', os.getpid())]])
            print 'Start queue for %i items in %i threads, master PID=%i'%(len(queue), copies, os.getpid())
            if _racs_params['_daemonize']: mixt.mk_daemon() #mixt.set_output()
            for q in queue:
                if len(pids)==copies:
                    p = os.waitpid(-1, 0)[0]
                    pids.remove(p)
                _args_from_racs = _base_args_from_racs+q #+[('master', os.getpid())]
                pid = os.fork()
                if not pid: break
                pids.append(pid)
            else:
                while(pids): pids.remove(os.waitpid(-1, 0)[0])
                sys.exit()
        elif _racs_params.get('_daemonize'): mixt.mk_daemon()
        #-----------------------------------------------------------------------
        for k, v in _args_from_racs: # накат сторонних параметров
            if k in self.__dict__: v = self.__dict__[k].__class__(v)
            self.__dict__[k] = v
        if 'path' in self.__dict__: # подготовка пути
            self.path = mixt.normpath(self.path)
            if self.path[-1]!='/': self.path += '/'
        _init_hook(self)
        if 'path' in self.__dict__ and os.path.exists(self.path+'.RACS'):
            self.__dict__.update(cPickle.load(open(self.path+'.RACS')))
    # def __repr__(self): return 'RACS(%r)'%self.path 
    # def __str__(self): return '@'+self.path #???
    #---------------------------------------------------------------------------
    def par_dict(self, *ignore_list): 
        return dict([(k, v) for k, v in self.__dict__.items() if k[0]!='_' and not k in _ignore_list+list(ignore_list)])
    #---------------------------------------------------------------------------
    def __getattr__(self, attr):
        'нужен для создания уникальной директории расчета по первому требованию (ленивые вычисления)'
        if attr=='path': return _make_path_hook(self)
        raise AttributeError(attr)
    #---------------------------------------------------------------------------
    def commit(self): 
        'Сохраняет содержимое расчета в базе' 
        cPickle.dump(dict(filter(lambda i:i[0][0]!='_' and i[0]!='path', self.__dict__.items())), 
                     open(self.path+'.RACS', 'w')) 
    #---------------------------------------------------------------------------
    def add_state(self, state, info=None, host=socket.gethostname(), login=mixt.get_login()):
        'Устанавливает статус расчета, НЕ вызывает commit()'
        import mixt, chrono 
        if not state in ('waited','activated','started','finished','stopped','suspended'):
            raise Exception('unknown status "%s" for "%s"'%(state, self.path if hasattr(self, 'path') else '???'))
        if info==None and state=='started': info = os.getpid()
        if info==None and state=='stopped': info = ''.join(mixt.except_report(None))
        if not hasattr(self, 'statelist'): self.statelist = []
        self.statelist.append((state, login, host, chrono.Date())+((info,) if info!=None else ()))
    def set_state(self, state, info=None, host=socket.gethostname(), login=mixt.get_login()):
        'Устанавливает статус расчета, вызывает commit()'
        self.add_state(state, info, host, login); self.commit()
    #---------------------------------------------------------------------------
    def set_progress(self, progress, runtime=-1, pbar_prompt=''):
        '''Устанавливает progress и runtime, выводит при необходимости mixt.ProgressBar. 
        prompt=@clean очищает ProgressBar, @close prompt result закрывает ProgressBar'''
        if not hasattr(self, 'statelist'): self.statelist = []
        runtime = (Date()-self.statelist[-1][3] if self.statelist else 0.) if runtime<0 else Time(runtime)
        self.__dict__['progress'], self.__dict__['runtime'] = progress, runtime
        if os.path.exists(self.path+'.RACS'): 
            L = open(self.path+'.RACS').readlines()
            if "sS'progress'\n" in L and "sS'runtime'\n" in L:
                L[L.index("sS'progress'\n")+2] = 'F%g\n'%progress
                L[L.index("sS'runtime'\n")+5] = 'F%g\n'%runtime
                open(self.path+'.RACS', 'w').write(''.join(L))
            else: self.commit() #self.md5sources = self.commit() ???
        if pbar_prompt:
            if not '_progressbar' in self.__dict__: self.__dict__['_progressbar'] = mixt.ProgressBar()
            if pbar_prompt=='@clean': self._progressbar.clean()  
            elif pbar_prompt.startswith('@close '): self._progressbar.close(*pbar_prompt[7:].rsplit(' ',1)) 
            else: self._progressbar.out(progress, pbar_prompt)
    #---------------------------------------------------------------------------
    # def commit_sources( self, *L ) : 
    #    'Сохраняет файлы с исходным кодом программы в базе'
    #    self.md5sources = sources.commit( self.path+'.sources.tar.gz', self.path+'.bzr_status', *L )
    #---------------------------------------------------------------------------
    def get_size(self): 'Размер расчета в байтах'; return int(os.popen("du -bs "+self.path).readline().split()[0])
    #---------------------------------------------------------------------------
    def push(self, X, ignore_list=[], _prefix='', **kw_args):
        '''устанавливает аттрибуты объекта X согласно объекту расчета, 
        параметры расчета имеют более высокий приоритет, чем параметры kw_args'''
        ignore_list = _ignore_list+(ignore_list.split() if type(ignore_list) is str else ignore_list)+['this', 'thisown']
        params = self.par_dict(*ignore_list).items()
        if _prefix: params = filter(lambda i: i[0].startswith(_prefix), params)
        if type(X) is dict: X.update(kw_args); X.update(params)
        elif hasattr(X, '__swig_setmethods__'): 
            for k, v in kw_args.items()+filter(lambda i: i[0] in X.__swig_setmethods__, params): setattr(X, k, v)
        else: 
            for k, v in kw_args.items()+params: setattr(X, k, v) 
    #---------------------------------------------------------------------------
    def pull(self, X, ignore_list=[], _prefix='', **kw_args):
        '''устанавливает аттрибуты объекта расчета согласно объекту X, 
        параметры kw_args имеют более высокий приоритет, чем параметры расчета
        автоматически устанавливаются аттрибуты имеющие методы __get/setstate__ 
        (но не имеющие аттрибута _racs_pull_lock) или не-являющиеся объектами swig'''
        ignore_list = _ignore_list+(ignore_list.split() if type(ignore_list) is str else ignore_list)+['this','thisown']
        if type(X) is dict: 
            for k, v in X.items():
                if not k in ignore_list and type(v) in (int,float,long,bool): self[k] = v
        else:
            for k in X.__dict__.keys()+getattr(X, '__swig_getmethods__', {}).keys():
                if not k in ignore_list+['__doc__']: 
                    v = getattr(X, k)
                    if all([hasattr(v, '__%setstate__'%a) for a in 'gs']+
                           [not hasattr(v, '_racs_pull_lock')]) or not _is_swig_obj(v): self[_prefix+k] = v
        for k, v in kw_args.items(): self[k] = v
    #---------------------------------------------------------------------------
    def wrap(self, core, prefix=''): return _Wrap(self, core, prefix)
    #---------------------------------------------------------------------------
    _except_report_table = []
    def __call__(self, expr):        
        try:
            if type(expr) is str: 
                if expr.startswith('@'): k, v = expr.split('=', 1); self[k] = v
                elif expr.startswith('$'): return ' '.join(os.popen(expr[1:]%self).readlines()).strip() 
                elif expr.endswith('!!'): exec(expr[:-2], dict(self.__dict__), _G)  
                elif expr.endswith('!'): exec(expr[:-1], _G, self)  
                else: return eval(expr, _G, self) 
            else: return eval(expr, dict(self.__dict__), _G) if expr.co_filename.endswith('!!') else eval(expr, _G, self)
        except Exception, e: 
            if _G['on_racs_call_error']==0: raise
            elif _G['on_racs_call_error'] in (1,2): 
                report = ''.join(mixt.except_report(None, short=_G['on_racs_call_error']-1))
                if not report in self._except_report_table: self._except_report_table.append(report)
    #---------------------------------------------------------------------------
    #def __getitem__(self, arg):
    #    'для форматирования строки'
    #    if '?' in arg:
    #        flag, arg = arg.split('?', 1)
    #        return arg%self if eval(flag, dict(math.__dict__), self.__dict__) else ''
    #    auto = arg.endswith('=')
    #    if auto: arg = arg[:-1]
    #    val = eval(arg, dict(math.__dict__), self.__dict__)
    #    if auto and val.__class__==bool: return ',%s'%arg if val else ''
    #    if isinstance(val, float): val = '%g'%val
    #    return ',%s=%s'%(arg, val) if auto else val
    def __getitem__(self, key):
        if key=='self': return self
        if key in self.__dict__: return self.__dict__[key]
        if key in _G or key in __builtins__: raise KeyError(key)        
        ak = '@'+key; c = key if not key.replace('_','').isalnum() else self.__dict__[ak] if ak in self.__dict__ else _G.get(ak)

        if key and not (key[0]=='_' or key[0].isalpha() and key.replace('_','0').isalnum()): return self(key)
        if c!= None:
            if key in _rtable: print>>sys.stderr, 'For "%s" recursion cropped'%key; return
            try: _rtable.append(key); return self(c)
            finally: _rtable.pop()
        if key=='runtime': return Time(0.) #???
	if key=='statelist': return
        #for r, a in _getitem_rules: 
        #    if r(key, self): return a(key, self)
        raise KeyError(key)        
    def __setitem__(self, key, val): self.__dict__[key] = val # блокировать доступ к statelist и пр???
    def __delitem__(self, key): del self.__dict__[key]
    def get(self, name, value=None): return self[name] if name in self.__dict__ else value
    def __contains__(self, key): return key in self.__dict__
    #---------------------------------------------------------------------------
#    def __setattr__(self, attr, value):
#        if attr in self.__dict__: self.__dict__[attr] = value.__class__(self.__dict__[attr])
#        else: self.__dict__[attr] = value
#-------------------------------------------------------------------------------
class _Wrap: 
    def __init__(self, calc, core, prefix):
        self.__dict__['_calc'], self.__dict__['_core'] = calc, core 
        self.__dict__['_set_attrs'], self.__dict__['_prefix'] = set(), prefix
        if hasattr(core, 'this'): self.__dict__['this'] = core.this # easy link to SWIG class O_O!
        if _racs_params['_auto_pull']: calc._wraps.append(self) # for exit hook
    def __getattr__(self, attr): return getattr(self._core, attr)
    def __setattr__(self, attr, value):
        if not attr in self._set_attrs and self._prefix+attr in self._calc.__dict__: # перекрываем значениe по умолчанию            
            value = self._calc.__dict__[self._prefix+attr] # через getattr?
            if getattr(self._core, attr).__class__==bool and type(value) is str: value = mixt.string2bool(value)
            value = getattr(self._core, attr).__class__(value)            
        self._set_attrs.add(attr)
        self._calc.__dict__[attr] = value
        setattr(self._core, attr, value)
#-------------------------------------------------------------------------------
