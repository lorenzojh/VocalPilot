-- Run after adding VocalPilot/bin to REAPER's VST scan path and rescanning.
-- Creates a new project tab; it does not change or save existing project tabs.
local _, path = reaper.get_action_context()
local folder = path:match('^(.*)[/\\]')
reaper.Main_OnCommand(40859, 0) -- New project tab
reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
reaper.GetSetMediaTrackInfo_String(track, 'P_NAME', 'VocalPilot: 432 Hz to A4', true)
local fx = reaper.TrackFX_AddByName(track, 'VST3: VocalPilot', false, -1)
if fx < 0 then
  reaper.ShowMessageBox('VocalPilot was not found. Add the bin folder to VST paths and re-scan.', 'VocalPilot smoke test', 0)
  return
end
reaper.SetOnlyTrackSelected(track)
reaper.InsertMedia(folder .. '/input-432.wav', 0)
reaper.SetEditCurPos(0, false, false)
reaper.GetSet_LoopTimeRange(true, false, 0, 3, false)
reaper.TrackFX_Show(track, fx, 3)
reaper.UpdateArrange()
reaper.ShowConsoleMsg('VocalPilot loaded. Press Play: 432 Hz should move toward A4 (440 Hz). Toggle Bypass to compare.\n')
