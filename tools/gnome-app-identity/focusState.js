// Pure metadata state, shared with the headless tests. No window titles/PIDs.
export function validAppId(id) {
    return typeof id === 'string' && id.length <= 120 && !/[^A-Za-z0-9_.-]/.test(id) &&
        /^[A-Za-z0-9_-][A-Za-z0-9_.-]*\.desktop$/.test(id);
}
export class FocusState {
    constructor(session) {
        this.session = session;
        this.serial = 1;
        this.appId = '';
        this.window = null;
    }
    update(appId, window, blocked = false) {
        const id = !blocked && validAppId(appId) ? appId : '';
        const target = blocked ? null : window;
        if (id === this.appId && target === this.window)
            return false;
        this.appId = id;
        this.window = target;
        this.serial = this.serial === 0xffffffff ? 1 : this.serial + 1;
        return true;
    }
    snapshot() { return [this.appId, this.session, this.serial]; }
}
