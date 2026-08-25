/* ==========================================================
   game-camera.js — who is playing.

   A name and a picture, kept in this browser's own storage.
   Nothing is uploaded and nothing leaves the machine.

   (The live in-game camera that used to live here has been
   removed. The name is kept so the two pages' script tags
   do not need changing.)
   ========================================================== */
(function (root) {

const PROFILE_KEY = 'chamkiForest.profile.v1';

/* ---- who is playing ---------------------------------------------------
   Kept separate from the sign-in session: signing out should not throw
   away a child's picture, and changing your picture is not signing in. */
const Profile = {
  read() {
    try {
      const d = JSON.parse(localStorage.getItem(PROFILE_KEY) || 'null');
      if (d && typeof d === 'object') return d;
    } catch (e) {}
    return { name: '', pic: '' };
  },
  save(d) {
    try { localStorage.setItem(PROFILE_KEY, JSON.stringify(d)); } catch (e) {}
  },
  setName(n) { const d = this.read(); d.name = String(n || '').slice(0, 16); this.save(d); return d; },
  setPic(dataUrl) { const d = this.read(); d.pic = dataUrl || ''; this.save(d); return d; },
  clearPic() { return this.setPic(''); },
  name() { return this.read().name || ''; },
  pic() { return this.read().pic || ''; },

  /* A data URL cannot go in an inline style: the framework splits the
     style string on ";" and "data:image/jpeg;base64,..." has one right
     in the middle, so the background came out as "data:image/jpeg".
     The bytes still live in storage as a data URL — this hands out a
     blob: URL for the same bytes, which has no ";" in it. */
  _objUrl: '', _objFrom: '',
  picUrl() {
    const d = this.pic();
    if (!d) { this._release(); return ''; }
    if (d === this._objFrom) return this._objUrl;
    this._release();
    try {
      const c = d.indexOf(',');
      const type = d.slice(5, c).split(';')[0] || 'image/jpeg';
      const bin = atob(d.slice(c + 1));
      const buf = new Uint8Array(bin.length);
      for (let i = 0; i < bin.length; i++) buf[i] = bin.charCodeAt(i);
      this._objUrl = URL.createObjectURL(new Blob([buf], { type: type }));
      this._objFrom = d;
    } catch (e) { this._objUrl = ''; this._objFrom = ''; }
    return this._objUrl;
  },
  _release() {
    if (this._objUrl) { try { URL.revokeObjectURL(this._objUrl); } catch (e) {} }
    this._objUrl = ''; this._objFrom = '';
  }
};

root.GameCamera = { Profile: Profile };

})(typeof window !== 'undefined' ? window : this);
