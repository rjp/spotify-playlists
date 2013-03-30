require 'xspf'

blobs = {}
tracks = Hash.new {|h,k| h[k] = Hash.new {|h,k| h[k] = Hash.new()}}
p = nil
t = nil
q = nil
metas = nil

# xspf blindly passes unescaped strings to eval in single quotes. Because ... yes. Why not.
class String
  def sq
    self.gsub(/'/, "\\\\'")
  end
end

c = 0

$stdin.readlines.each do |line|
    tag, ref, *stuff = line.force_encoding('utf-8').chomp.split(' ')
    t = tracks[ref] # current tracklist
    i = stuff[0].to_i
    case tag
        when 'PLAYLIST' then
            blobs[ref] = XSPF::Playlist.new
            p = blobs[ref]
            p.title = stuff[1..-1].join(' ').sq()
            p.tracklist = XSPF::Tracklist.new
        when 'PLAYLIST:END' then
            File.open("playlists/#{c}.xspf", "w") do |f|
                f.write p.to_xml
            end
            c = c + 1
            puts "Written #{c} #{p.title}"
        when 'OWNER' then
            p.creator = stuff[0]
        when 'TRACK:CREATOR' then 
            q = []
            metas = []
            metas << { :key => 'http://browser.org/xspf/spotify/added_by', :value => stuff[1] }
        when 'TRACK:URI' then
            t[i][:identifier] = stuff[1]
            t[i][:tracknum] = (i+1).to_s
        when 'TRACK:NAME' then
            t[i][:title] = stuff[1..-1].join(' ').sq()
        when 'TRACK:URI' then
            metas << { :key => 'http://browser.org/xspf/spotify/track', :value => stuff[1] }
        when 'TRACK:DURATION' then
            t[i][:duration] = stuff[1]
        when 'TRACK:EPOCH' then
            metas << { :key => 'http://browser.org/xspf/spotify/added_time', :value => stuff[1] }
        when 'ALBUM:NAME' then
            t[i][:album] = stuff[1..-1].join(' ').sq()
        when 'ALBUM:URI' then
            metas << { :key => 'http://browser.org/xspf/spotify/album', :value => stuff[1] }
        when 'ARTIST:NAME' then
            q << stuff[2..-1].join(' ').sq()
        when 'ARTIST:URI' then
            metas << { :key => 'http://browser.org/xspf/spotify/artist', :value => stuff[1] }
        when 'TRACK:END' then
            t[i][:creator] = q.join(', ')
            t[i][:metas] = metas
            x = XSPF::Track.new( t[i] )
            p.tracklist << x
    end
end
