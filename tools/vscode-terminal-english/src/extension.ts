import * as vscode from 'vscode';
import { setTimeout as settle } from 'node:timers/promises';
import { RemoteClient } from './remote';
import { TerminalSwitcher } from './switcher';

export function activate(context: vscode.ExtensionContext): void {
  let disposed = false;
  let ownedTerminal: vscode.Terminal | undefined;
  const enabled = () => vscode.workspace.getConfiguration('openkeyTerminalEnglish').get<boolean>('enabled', false);
  const allowed = () => !disposed && process.platform === 'linux' &&
    vscode.env.uiKind === vscode.UIKind.Desktop && enabled() && vscode.window.state.focused;
  const client = new RemoteClient(allowed);
  const switcher = new TerminalSwitcher({
    isFocused: allowed,
    read: () => client.read(),
    deactivate: () => client.deactivate(),
    activate: method => client.activate(method),
    focus: async target => {
      const signal = client.signal();
      client.check(signal);
      await vscode.commands.executeCommand(target === 'terminal' ?
        'workbench.action.terminal.focus' : 'workbench.action.focusActiveEditorGroup');
      // The command resolving is not an Fcitx input-context acknowledgement.
      // A bounded settling interval helps, but still requires desktop QA.
      await settle(100, undefined, { signal });
      client.check(signal);
      if (target === 'terminal') {
        if (!vscode.window.activeTerminal || vscode.window.activeTerminal.exitStatus !== undefined)
          throw new Error('No active terminal');
        ownedTerminal = vscode.window.activeTerminal;
      }
    },
  });
  const invalidate = () => { client.cancel(); switcher.invalidate(); ownedTerminal = undefined; };
  const notify = (result: string) => {
    if (!allowed()) return;
    if (['error', 'unsupported', 'changed'].includes(result)) {
      void vscode.window.showWarningMessage(result === 'changed' ?
        'OpenKey: Trạng thái bộ gõ đã đổi; không tự ghi đè. Chọn EN/VI thủ công nếu cần.' :
        'OpenKey: Không chuyển/khôi phục được Fcitx5. Kiểm tra bộ gõ trên máy giao diện; không tự thử lại.');
    } else if (!['busy', 'cancelled', 'unfocused'].includes(result)) {
      vscode.window.setStatusBarMessage('OpenKey: ' + (result === 'entered' ?
        'terminal đã chuyển EN' : 'đã xử lý phím chuyển vùng; kiểm tra EN/VI trên panel'), 2500);
    }
  };
  context.subscriptions.push(
    vscode.commands.registerCommand('openkeyTerminalEnglish.focusTerminal', async () => {
      if (!allowed()) return;
      // An explicit new editor -> terminal command starts a fresh cycle if
      // the user previously returned by mouse. Never restore that stale snapshot.
      if (ownedTerminal !== undefined) invalidate();
      notify(await switcher.enter());
    }),
    vscode.commands.registerCommand('openkeyTerminalEnglish.focusEditor', async () => {
      if (!allowed()) return;
      if (ownedTerminal !== undefined && ownedTerminal !== vscode.window.activeTerminal) invalidate();
      const result = await switcher.leave();
      notify(result);
      if (result !== 'busy') ownedTerminal = undefined;
    }),
    vscode.window.onDidChangeWindowState(state => { if (!state.focused) invalidate(); }),
    vscode.window.onDidCloseTerminal(terminal => { if (terminal === ownedTerminal) invalidate(); }),
    vscode.workspace.onDidChangeConfiguration(event => {
      if (event.affectsConfiguration('openkeyTerminalEnglish.enabled')) invalidate();
    }),
    new vscode.Disposable(() => { disposed = true; invalidate(); switcher.dispose(); }),
  );
}
