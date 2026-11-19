"""make_license_gui.pyw - the support license (make_license.py) in a window: the unit's ID as VestaHome shows it,
the level, "Создать ключ…" -> VestaLicense.key where you choose. The author's private key: by default
%USERPROFILE%\\.vesta\\vesta_license_private.pem ("Другой ключ…" to pick another). Needs "cryptography".

    pythonw VestaCore/tools/make_license_gui.pyw    (or a double click)
"""
import os
import sys
import tkinter as tk
from tkinter import filedialog, ttk

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_license as ml  # noqa: E402

LEVELS = ((1, 'Подписчик — убирает заставку в Android Auto'),
          (2, 'Соавтор — заставка Android Auto и «Умный дом»'))


class App:
    def __init__(self, root):
        self.root = root
        root.title('Ключ поддержки VestaHome')
        root.resizable(False, False)
        f = ttk.Frame(root, padding=16)
        f.grid()
        ttk.Label(f, text='ID головного устройства («О системе» в VestaHome):').grid(sticky='w')
        self.id = tk.StringVar()
        self.id.trace_add('write', self.on_id)
        e = ttk.Entry(f, textvariable=self.id, width=36, font=('Consolas', 14))
        e.grid(sticky='we', pady=(4, 0))
        e.focus()
        self.idNote = ttk.Label(f, text=' ', foreground='gray')
        self.idNote.grid(sticky='w', pady=(2, 10))
        ttk.Label(f, text='Уровень:').grid(sticky='w')
        self.level = tk.IntVar(value=1)
        for v, t in LEVELS:
            ttk.Radiobutton(f, text=t, variable=self.level, value=v).grid(sticky='w')
        ttk.Label(f, text='Закрытый ключ автора:').grid(sticky='w', pady=(10, 0))
        k = ttk.Frame(f)
        k.grid(sticky='we')
        self.priv = ml.key_path(None)
        self.privLabel = ttk.Label(k, text=self.priv, foreground='gray')
        self.privLabel.pack(side='left')
        ttk.Button(k, text='Другой ключ…', command=self.pick_priv).pack(side='right', padx=(8, 0))
        self.make = ttk.Button(f, text='Создать ключ…', command=self.create, state='disabled')
        self.make.grid(sticky='we', pady=(14, 6), ipady=4)
        self.status = ttk.Label(f, text=self.priv_note(), wraplength=460)
        self.status.grid(sticky='w')
        root.bind('<Return>', lambda _e: self.create())

    def priv_note(self):
        return '' if os.path.exists(self.priv) else 'Закрытого ключа нет по этому пути — выберите «Другой ключ…».'

    def on_id(self, *_):
        uid = ml.parse_id(self.id.get())
        if uid:
            self.idNote.config(text='будет: ' + uid, foreground='green')
        else:
            self.idNote.config(text='вид: VH-XXXXX-XXXXX' if self.id.get().strip() else ' ', foreground='gray')
        self.make.config(state='normal' if uid else 'disabled')

    def pick_priv(self):
        p = filedialog.askopenfilename(title='Закрытый ключ автора', filetypes=[('PEM', '*.pem'), ('Все', '*.*')],
                                       initialdir=os.path.dirname(self.priv))
        if p:
            self.priv = os.path.normpath(p)
            self.privLabel.config(text=self.priv)
            self.status.config(text=self.priv_note(), foreground='black')

    def create(self):
        uid = ml.parse_id(self.id.get())
        if not uid:
            return
        out = filedialog.asksaveasfilename(title='Сохранить ключ для ' + uid, initialfile='VestaLicense.key',
                                           initialdir=os.path.join(os.path.expanduser('~'), 'Desktop'),
                                           defaultextension='.key', filetypes=[('Ключ', '*.key')])
        if not out:
            return
        try:
            ml.write_key(self.priv, uid, self.level.get(), out)
        except (ValueError, OSError) as e:
            self.status.config(text='Не получилось: %s' % e, foreground='red')
            return
        self.status.config(text='Готово: %s (%s, %s).\nНа ГУ — в \\ResidentFlash\\VestaHome\\, имя VestaLicense.key.'
                           % (os.path.normpath(out), uid, 'Соавтор' if self.level.get() == 2 else 'подписчик'),
                           foreground='green')


if __name__ == '__main__':
    root = tk.Tk()
    App(root)
    root.mainloop()
