import React, { useState, useEffect, useMemo, useCallback, memo } from 'react';
import { 
  Droplets, Plus, Clock, Trash2, List, X, History, CheckCircle2, AlertCircle, Activity, 
  BarChart3, Sun, Moon, Sunrise, Sunset, MoreHorizontal, MapPin, Calendar, Loader2 
} from 'lucide-react'; 

// --- CONSTANTS ---
const DAYS = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'];
const AREAS = ['Front Lawn', 'Back Garden', 'Flower Bed', 'Veg Patch', 'Greenhouse'];
const TIME_PRESETS = [
  { label: 'Morning', time: '06:00', icon: Sunrise },
  { label: 'Day', time: '08:00', icon: Sun },
  { label: 'Noon', time: '12:00', icon: Sun },
  { label: 'Late', time: '16:00', icon: Sunset },
  { label: 'Evening', time: '18:00', icon: Moon },
  { label: 'Night', time: '20:00', icon: Moon },
];
const LOGO_FILE = "SIM_FLOW_LOGO.jpg"; 
const INITIAL_SCHEDULES = [
  { id: 1, days: ['MON', 'WED', 'FRI'], time: '07:00', duration: 30, area: 'Front Lawn' },
  { id: 2, days: ['SAT', 'SUN'], time: '18:30', duration: 60, area: 'Flower Bed' },
];
const MOCK_HISTORY = [
  { id: 101, day: 'SUN', dayIndex: 0, time: '07:00', liters: 12, status: 'completed' },
  { id: 102, day: 'MON', dayIndex: 1, time: '07:00', liters: 15, status: 'completed' },
  { id: 103, day: 'WED', dayIndex: 3, time: '07:00', liters: 0, status: 'skipped' }, 
  { id: 104, day: 'FRI', dayIndex: 5, time: '18:30', liters: 25, status: 'completed' },
  { id: 105, day: 'SAT', dayIndex: 6, time: '08:15', liters: 10, status: 'completed' },
];

// --- API ---
const hardwareApi = {
  createSchedule: async (data) => {
    const API_URL = "http://localhost:5000/api/schedule";
    try {
      const response = await fetch(API_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(data),
      });
      if (!response.ok) throw new Error(`Device sync failed: ${response.statusText}`);
      return await response.json(); 
    } catch (error) {
      console.error("API Error:", error);
      throw error;
    }
  },
  deleteSchedule: async (scheduleId) => {
    const API_URL = `http://localhost:5000/api/schedule/${scheduleId}`;
    try {
      const response = await fetch(API_URL, {
        method: 'DELETE',
        headers: { 'Content-Type': 'application/json' },
      });
      if (!response.ok) throw new Error(`Delete failed: ${response.statusText}`);
      return await response.json();
    } catch (error) {
      console.error("API Error:", error);
      throw error;
    }
  },
  getSchedules: async () => {
    const API_URL = "http://localhost:5000/api/schedules";
    try {
      const response = await fetch(API_URL, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' },
      });
      if (!response.ok) throw new Error(`Failed to fetch schedules: ${response.statusText}`);
      const data = await response.json();
      return data.schedules || [];
    } catch (error) {
      console.error("API Error:", error);
      return [];
    }
  }
};

// --- HOOKS ---
const useWateringSystem = (initialSchedules) => {
  const [schedules, setSchedules] = useState(initialSchedules);
  const [now, setNow] = useState(new Date());
  const [simulatingActive, setSimulatingActive] = useState(false);

  useEffect(() => {
    const interval = setInterval(() => setNow(new Date()), 1000 * 60); 
    return () => clearInterval(interval);
  }, []);

  // Load saved schedules from backend on app startup
  useEffect(() => {
    const loadSchedules = async () => {
      try {
        const savedSchedules = await hardwareApi.getSchedules();
        if (savedSchedules.length > 0) {
          setSchedules(savedSchedules);
          console.log('Loaded schedules from backend:', savedSchedules);
        }
      } catch (error) {
        console.error('Failed to load schedules:', error);
      }
    };
    loadSchedules();
  }, []);

  const addSchedule = useCallback(async (newScheduleData) => {
    const sortedDays = [...newScheduleData.days].sort((a, b) => DAYS.indexOf(a) - DAYS.indexOf(b));
    const payload = { ...newScheduleData, days: sortedDays };
    try {
      const responseData = await hardwareApi.createSchedule(payload);
      if (responseData.status === 'error') {
        alert(`Schedule creation failed: ${responseData.message}`);
        return false;
      }
      // Use hardware ID from response instead of generating one
      const scheduleWithId = { ...responseData, id: responseData.id };
      setSchedules(prev => [...prev, scheduleWithId]);
      return true;
    } catch (error) {
      alert("Failed to connect to SimFlow Middleware. Is backend.py running?");
      return false;
    }
  }, []);

  const deleteSchedule = useCallback(async (id) => {
    try {
      await hardwareApi.deleteSchedule(id);
      setSchedules(prev => prev.filter(s => s.id !== id));
    } catch (error) {
      alert(`Failed to delete schedule: ${error.message}`);
    }
  }, []);
  const toggleSimulation = useCallback(() => setSimulatingActive(prev => !prev), []);

  const activeSchedule = useMemo(() => {
    if (simulatingActive) return { days: ['MON'], time: '00:00', duration: 30, area: 'Demo Area', activeNow: true };
    const currentDayIndex = now.getDay(); 
    const currentDayStr = DAYS[currentDayIndex];
    const currentMinutes = now.getHours() * 60 + now.getMinutes();
    return schedules.find(s => {
      if (!s.days.includes(currentDayStr)) return false;
      const [h, m] = s.time.split(':').map(Number);
      const start = h * 60 + m;
      const end = start + parseInt(s.duration);
      return currentMinutes >= start && currentMinutes < end;
    });
  }, [schedules, now, simulatingActive]);

  const nextEvent = useMemo(() => {
    if (!schedules.length) return null;
    let next = null;
    let minDiff = Infinity;
    const currentDayIndex = now.getDay();
    const currentMinutes = now.getHours() * 60 + now.getMinutes();
    schedules.forEach(schedule => {
      if (!schedule.days?.length) return;
      const [h, m] = schedule.time.split(':').map(Number);
      const schedMinutes = h * 60 + m;
      let daysUntil = -1;
      let targetDayStr = '';
      if (schedule.days.includes(DAYS[currentDayIndex]) && schedMinutes > currentMinutes) {
         daysUntil = 0; targetDayStr = 'Today';
      } else {
         for (let i = 1; i <= 7; i++) {
            const nextIndex = (currentDayIndex + i) % 7;
            const checkDay = DAYS[nextIndex];
            if (schedule.days.includes(checkDay)) {
               daysUntil = i; targetDayStr = i === 1 ? 'Tomorrow' : checkDay; break;
            }
         }
      }
      if (daysUntil !== -1) {
         const totalMinutesUntil = (daysUntil * 24 * 60) + (schedMinutes - currentMinutes);
         if (totalMinutesUntil < minDiff) {
            minDiff = totalMinutesUntil; next = { ...schedule, displayDay: targetDayStr };
         }
      }
    });
    return next;
  }, [schedules, now]);

  return { now, schedules, activeSchedule, nextEvent, addSchedule, deleteSchedule, simulatingActive, toggleSimulation };
};

// --- COMPONENTS ---
const BrandLogo = memo(({ className }) => {
  const [error, setError] = useState(false);
  if (error) return (
      <svg viewBox="0 0 100 100" className={className} fill="none" stroke="currentColor" strokeWidth="10" strokeLinecap="round">
        <path d="M70 30 C 70 10, 30 10, 30 30 C 30 60, 70 60, 70 80" className="text-teal-600" stroke="currentColor"/>
        <path d="M60 30 C 60 18, 40 18, 40 30 C 40 50, 60 50, 60 80" className="text-slate-800" stroke="currentColor" strokeOpacity="0.8" strokeWidth="6"/>
        <circle cx="70" cy="80" r="5" fill="currentColor" className="text-lime-500" stroke="none"/>
      </svg>
    );
  return <img src={LOGO_FILE} alt="SimFlow Logo" className={`${className} object-contain rounded-xl`} onError={() => setError(true)} />;
});

const HistoryChart = memo(({ data }) => {
  const height = 250; const width = 1000; const padding = { top: 20, right: 20, bottom: 40, left: 140 }; 
  const maxVolume = 40; const availableWidth = width - padding.left - padding.right; const slotWidth = availableWidth / 7;
  const getY = (liters) => (height - padding.bottom) - ((Math.min(liters, maxVolume) / maxVolume) * (height - padding.top - padding.bottom));
  const getBarHeight = (liters) => (Math.min(liters, maxVolume) / maxVolume) * (height - padding.top - padding.bottom);
  return (
    <div className="w-full h-full min-h-[250px]">
       <svg viewBox={`0 0 ${width} ${height}`} className="w-full h-full overflow-visible" role="img" aria-label="History chart">
          {[0, 10, 20, 30, 40].map(L => (
             <g key={L}><line x1={padding.left} y1={getY(L)} x2="100%" y2={getY(L)} stroke="#e2e8f0" strokeWidth="1" strokeDasharray="4 4" /><text x={padding.left - 20} y={getY(L) + 8} textAnchor="end" fontSize="20" fill="#94a3b8" fontWeight="300" style={{ fontFamily: 'sans-serif' }}>{L}L</text></g>
          ))}
          {DAYS.map((day, i) => <text key={day} x={padding.left + (i * slotWidth) + (slotWidth / 2)} y={height - 5} textAnchor="middle" fontSize="20" fill="#64748b" fontWeight="300" style={{ fontFamily: 'sans-serif' }}>{day}</text>)}
          {data.map((d) => {
             const cx = padding.left + (d.dayIndex * slotWidth) + (slotWidth / 2);
             return <g key={d.id} className="group cursor-pointer"><rect x={cx - 25} y={getY(d.liters)} width="50" height={Math.max(getBarHeight(d.liters), 4)} rx="8" fill={d.liters===0?'#cbd5e1':'#84cc16'} className={`transition-all hover:opacity-80 ${d.liters===0?'opacity-50':''}`} /><text x={cx} y={getY(d.liters) - 12} textAnchor="middle" fill={d.liters===0?'#94a3b8':'#65a30d'} fontSize="18" fontWeight="400" style={{ fontFamily: 'sans-serif' }}>{d.liters===0?'Skip':`${d.liters}L`}</text></g>;
          })}
       </svg>
    </div>
  );
});

const SmartTimeGrid = ({ value, onChange }) => {
  const [isCustomMode, setIsCustomMode] = useState(false);
  return (
    <div className="bg-slate-50 border border-slate-200 rounded-xl p-4">
       {!isCustomMode ? (
         <div className="grid grid-cols-3 gap-3 mb-3">
           {TIME_PRESETS.map(p => (
             <button key={p.time} onClick={() => onChange(p.time)} className={`flex flex-col items-center justify-center p-3 rounded-xl border-2 transition-all ${value === p.time ? 'bg-white border-teal-500 text-teal-700 shadow-md ring-1 ring-teal-500' : 'bg-white border-transparent hover:border-slate-200 text-slate-500 hover:text-slate-700 shadow-sm'}`}>
                <p.icon size={18} className="mb-1 opacity-70" /><span className="text-sm font-bold">{p.time}</span><span className="text-[10px] uppercase font-bold opacity-50">{p.label}</span>
             </button>
           ))}
         </div>
       ) : (
          <div className="animate-in fade-in slide-in-from-top-2 duration-200">
             <div className="flex justify-between items-center mb-2"><span className="text-xs font-bold text-slate-400 uppercase">Custom Time</span><button onClick={() => setIsCustomMode(false)} className="text-xs font-bold text-teal-600 hover:underline">Back to Presets</button></div>
             <div className="relative flex items-center justify-center bg-white border border-teal-500 rounded-xl p-2 shadow-sm ring-2 ring-teal-500/10"><input type="time" value={value} onChange={(e) => onChange(e.target.value)} className="w-full bg-transparent border-none text-3xl font-bold text-slate-800 p-2 focus:ring-0 cursor-pointer font-mono tracking-tight text-center" /></div>
          </div>
       )}
       {!isCustomMode && <button onClick={() => setIsCustomMode(true)} className="w-full flex items-center justify-center gap-2 p-3 rounded-xl border border-slate-200 bg-white text-slate-400 font-bold text-xs hover:bg-slate-50 hover:text-slate-600 transition-colors"><MoreHorizontal size={16} /> Other Time...</button>}
    </div>
  );
};

const AddForm = ({ onSave }) => {
  const [formData, setFormData] = useState({ selectedDays: ['SUN'], newTime: '07:00', newDuration: 15, newArea: AREAS[0] });
  const [isSaving, setIsSaving] = useState(false);
  const toggleDay = (day) => setFormData(prev => {
      const days = prev.selectedDays;
      if (days.includes(day)) return days.length === 1 && days[0] === day ? prev : { ...prev, selectedDays: days.filter(d => d !== day) };
      return { ...prev, selectedDays: [...days, day] };
  });
  const updateField = (field, value) => setFormData(prev => ({ ...prev, [field]: value }));
  const handleSave = async () => {
    setIsSaving(true);
    await onSave({ days: formData.selectedDays, time: formData.newTime, duration: formData.newDuration, area: formData.newArea });
    setIsSaving(false);
  };
  return (
    <div className="space-y-8">
      <div><label className="text-slate-500 text-sm font-bold mb-3 block">Days of Week</label><div className="grid grid-cols-4 sm:grid-cols-7 gap-2">{DAYS.map(d => (<button key={d} onClick={() => toggleDay(d)} disabled={isSaving} className={`py-3 text-xs font-bold rounded-xl transition-all border-2 ${formData.selectedDays.includes(d) ? 'bg-teal-600 border-teal-600 text-white shadow-md' : 'bg-white border-slate-100 text-slate-400 hover:border-teal-200 hover:text-teal-600'} ${isSaving ? 'opacity-50' : ''}`}>{d}</button>))}</div></div>
      <div className="grid grid-cols-1 sm:grid-cols-2 gap-8">
        <div><label className="text-slate-500 text-sm font-bold mb-2 block">Start Time</label><div className={isSaving?'opacity-50 pointer-events-none':''}><SmartTimeGrid value={formData.newTime} onChange={(val) => updateField('newTime', val)} /></div></div>
        <div className="flex flex-col gap-6">
          <div><label className="text-slate-500 text-sm font-bold mb-2 block">Area</label><div className="grid grid-cols-2 gap-2">{AREAS.map(a => (<button key={a} onClick={() => updateField('newArea', a)} disabled={isSaving} className={`p-2 text-xs font-bold rounded-lg border text-left transition-all ${formData.newArea === a ? 'bg-teal-50 border-teal-500 text-teal-700' : 'bg-white border-slate-200 text-slate-500'} ${isSaving ? 'opacity-50' : ''}`}>{a}</button>))}</div></div>
          <div><label className="text-slate-500 text-sm font-bold mb-2 block">Duration</label><div className={`h-[90px] flex flex-col justify-center bg-slate-50 border border-slate-200 p-4 rounded-xl ${isSaving?'opacity-50':''}`}><div className="flex justify-between items-end mb-2"><div><span className="text-3xl font-bold text-teal-700">{formData.newDuration}</span><span className="text-xs text-slate-400 font-bold ml-1 uppercase">MIN</span></div></div><input type="range" min="1" max="180" value={formData.newDuration} disabled={isSaving} onChange={(e) => updateField('newDuration', parseInt(e.target.value))} className="w-full accent-teal-600 cursor-pointer" /></div></div>
        </div>
      </div>
      <div className="pt-4 flex gap-3"><button onClick={handleSave} disabled={isSaving} className="flex-1 bg-slate-800 hover:bg-slate-900 text-white font-bold p-4 rounded-xl shadow-lg flex justify-center items-center gap-2">{isSaving ? <Loader2 className="animate-spin" size={20} /> : <CheckCircle2 size={20} />}{isSaving ? 'Connecting...' : 'Save Schedule'}</button></div>
    </div>
  );
};

const DashboardView = ({ schedules, activeSchedule, nextEvent, onDelete }) => (
    <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
      <div className="lg:col-span-2 bg-white rounded-2xl p-6 shadow-sm border border-slate-200 flex flex-col">
          <div className="flex justify-between items-center mb-6"><div><h2 className="text-lg font-bold text-slate-800 flex items-center gap-2"><BarChart3 className="text-teal-500" size={20} />Watering History</h2></div></div>
          <div className="flex-1 min-h-[250px] w-full"><HistoryChart data={MOCK_HISTORY} /></div>
      </div>
      <div className="bg-white p-6 rounded-2xl shadow-sm border border-slate-200 flex flex-col justify-between relative overflow-hidden min-h-[300px]">
          {activeSchedule && (<div className="absolute top-0 left-0 w-full h-1 bg-teal-500 animate-pulse"></div>)}
          <div><h3 className="text-slate-400 text-xs font-bold uppercase tracking-wider mb-4">{activeSchedule ? 'Currently Active' : 'Up Next'}</h3>{activeSchedule ? (<div className="text-center py-4"><div className="w-16 h-16 bg-teal-50 rounded-full flex items-center justify-center mx-auto mb-3 text-teal-500"><Droplets size={32} className="animate-bounce" /></div><h2 className="text-2xl font-bold text-teal-600">Watering</h2><p className="text-slate-500 font-mono text-sm mt-1">{activeSchedule.area} • <span className="text-slate-800 font-bold">{activeSchedule.duration}m</span> remaining</p></div>) : (nextEvent ? (<div className="animate-in fade-in duration-500"><div className="text-4xl font-bold text-slate-800 mb-1 font-mono">{nextEvent.time}</div><div className="text-teal-600 font-bold text-lg mb-6 truncate flex items-center gap-2"><Calendar size={18} />{nextEvent.displayDay}</div><div className="flex items-center gap-2 text-slate-500 text-sm bg-slate-50 p-3 rounded-lg"><Clock size={16} /><span>Duration: {nextEvent.duration} min</span></div><div className="mt-2 text-xs text-slate-400 flex items-center gap-1"><MapPin size={12} />{nextEvent.area}</div></div>) : (<div className="py-8 text-center text-slate-400 italic">No schedules set</div>))}</div>
      </div>
      <div className="bg-white rounded-2xl shadow-sm border border-slate-200 overflow-hidden lg:col-span-3">
        <div className="p-6 border-b border-slate-100"><h3 className="text-lg font-bold text-slate-800">Active Schedules</h3></div>
        <div className="p-0"><table className="w-full text-left border-collapse"><thead className="bg-slate-50 text-slate-500 text-xs uppercase border-b border-slate-100"><tr><th className="p-4 w-1/3">Days</th><th className="p-4">Area</th><th className="p-4">Time</th><th className="p-4">Duration</th><th className="p-4 text-right">Actions</th></tr></thead><tbody className="divide-y divide-slate-100">{schedules.map(s => (<tr key={s.id} className="hover:bg-slate-50/80"><td className="p-4 font-bold text-slate-700 text-sm">{s.days.join(', ')}</td><td className="p-4 text-slate-600 text-sm">{s.area}</td><td className="p-4 font-mono text-teal-600 text-sm">{s.time}</td><td className="p-4 text-slate-500 text-sm">{s.duration} min</td><td className="p-4 text-right"><button onClick={() => onDelete(s.id)} className="p-2 text-slate-300 hover:text-red-500"><Trash2 size={18} /></button></td></tr>))}</tbody></table></div>
      </div>
    </div>
);

export default function App() {
  const { now, schedules, activeSchedule, nextEvent, addSchedule, deleteSchedule, simulatingActive, toggleSimulation } = useWateringSystem(INITIAL_SCHEDULES);
  const [view, setView] = useState('list'); 
  return (
    <div className="bg-slate-50 min-h-screen text-slate-800 font-sans flex flex-col md:flex-row">
      <aside className="hidden md:flex flex-col items-center py-8 w-24 bg-white border-r border-slate-200 shadow-sm z-10"><div className="mb-10 text-teal-600"><BrandLogo className="w-10 h-10" /></div><nav className="flex flex-col gap-6 w-full px-4"><button onClick={() => setView('list')} className={`p-3 rounded-xl flex flex-col items-center gap-1 ${view==='list'?'bg-teal-50 text-teal-600':'text-slate-400 hover:text-slate-600'}`}><List size={24}/><span className="text-[10px] font-bold">Dash</span></button><button onClick={() => setView('add')} className={`p-3 rounded-xl flex flex-col items-center gap-1 ${view==='add'?'bg-teal-50 text-teal-600':'text-slate-400 hover:text-slate-600'}`}><Plus size={24}/><span className="text-[10px] font-bold">New</span></button><button onClick={toggleSimulation} className={`mt-8 p-3 rounded-xl flex flex-col items-center gap-1 border ${simulatingActive?'bg-blue-50 border-blue-200 text-blue-600':'border-slate-100 text-slate-300'}`}><Activity size={20}/><span className="text-[10px] font-bold">Demo</span></button></nav></aside>
      <main className="flex-1 flex flex-col h-screen overflow-hidden"><header className="bg-white px-6 py-5 border-b border-slate-200 flex justify-between items-center shrink-0"><div className="flex items-center gap-3"><div className="md:hidden text-teal-600"><BrandLogo className="w-8 h-8" /></div><h1 className="text-2xl font-bold tracking-tight text-slate-800">SimFlow Dashboard</h1></div><button onClick={() => setView(view==='list'?'add':'list')} className="md:hidden bg-teal-600 text-white p-2 rounded-lg"><Plus size={20}/></button><div className="hidden md:flex items-center gap-4 text-sm text-slate-500 font-medium"><span>{now.toLocaleDateString()}</span><div className="w-8 h-8 bg-teal-100 rounded-full flex items-center justify-center text-teal-700 font-bold">A</div></div></header><div className="flex-1 overflow-y-auto p-4 md:p-8 space-y-6 bg-slate-50/50">{view === 'add' ? (<div className="bg-white rounded-2xl shadow-sm border border-slate-200 overflow-hidden p-6 max-w-2xl mx-auto"><div className="border-b border-slate-100 pb-4 mb-6 flex justify-between items-center"><h3 className="text-lg font-bold text-slate-800">Configure New Schedule</h3><button onClick={() => setView('list')} className="text-slate-400 hover:text-slate-600"><X size={24}/></button></div><AddForm onSave={async (data) => { await addSchedule(data); setView('list'); }} /></div>) : (<DashboardView schedules={schedules} activeSchedule={activeSchedule} nextEvent={nextEvent} onDelete={deleteSchedule} />)}</div></main>
    </div>
  );
}
