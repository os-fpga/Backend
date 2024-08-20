"""maintain sets with union and find operators"""

class uf:
    """implement the UNION-FIND algorithm and its data structure"""
    def __init__(self):
        self._dict = {}
    def add(self, a):
        """add a to the set"""
        if a not in self._dict: self._dict[a] = [1, a]
    def find(self, a):
        """find a's set"""
        if a not in self._dict: return a
        root = a
        while root != self._dict[root][1]:
            root = self._dict[root][1]
        while root != self._dict[a][1]:
            a2 = self._dict[a][1]
            self._dict[a][1] = root
            a = a2
        return root
    def union(self, a, b):
        """put a and b in the same set"""
        self.add(a); self.add(b)
        a = self.find(a); b = self.find(b)
        if a == b: return
        if self._dict[a][0] > self._dict[b][0]: a, b = b, a
        self._dict[a][1] = b
        self._dict[b][0] += self._dict[a][0]
    def keys(self):
        """return as all entries as keys"""
        return self._dict.keys()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch
# vim: filetype=python
# vim: syntax=on
# vim: fileformat=unix
