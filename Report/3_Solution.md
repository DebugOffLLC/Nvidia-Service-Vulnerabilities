## Section Object

The easiest solution for the section object is to create it in the root `\BaseNamedObjects` directory. Normal users cannot create section objects nor links in this location, but they can create other types. If an object already exists in the root BNO directory with the same name, then either it will be the wrong type and creating the file mapping will fail, or it was created by a high-integrity process in which case a security boundary is no longer crossed (admin to system).

I'm not sure what the full scope of the use case is for this object, but making the permissions more strict is recommended. Additionally, a thorough audit of how user-controllable fields are used within the hooks would be wise.

## Hook Callbacks

Refer to the PoC for what I think the object layout is. Specifically, the Dump project.

### Race Conditions

This is going to be a case-by-case basis and take far too much effort for me to attempt to RE. As such, I'm leaving it as a note.

Several of the bugs below can be fixed with a simple check. However, these checks may still leave a gap for race conditions by changing the value after it is checked, but before it is used. As such, in addition to locks, certain operations may benefit from the creation of a local copy of data to operate on rather than reading from the shared memory repeatedly.

```c
// Instead of
if(0x10 == pSharedMem->value){
    pSharedMem->value += 1;
}

// Do
DWORD dwValue = pSharedMem->value
if(0x10 == dwValue){
    dwValue += 1;
}
```

The above example is unlikely to be impacted by a race condition since it completes very quickly, however, in loops and longer functions this becomes more relevant.

This does become quite complicated, however, since at some points you may want to retrieve updated info from the memory section.

### Div By Zero

The desktop `RECT` is pulled from the shared memory, then its values are used for division. This can result in a div by zero.

```c
18001e43b  __builtin_memcpy(dest: &rDesktop, src: &pDisplayEntry->rcDesktop, 
18001e43b      count: 0x10)
18001e44d  SmartMax_ClipRectToExclusionZones(pDisplayEntry, lprcDst: &rDesktop, 
18001e44d      nFlag: 0)
18001e486  int32_t temp0_1 = divs.dp.d(
18001e486      sx.q((rDesktop.bottom - rDesktop.top)
18001e486          * (rTaskbar.right - rTaskbar.left)), 
18001e486      rDesktop.right - rDesktop.left)
18001e4c3  int32_t temp0_2 = divs.dp.d(
18001e4c3      sx.q((rDesktop.right - rDesktop.left)
18001e4c3          * (rTaskbar.bottom - rTaskbar.top)), 
18001e4c3      rDesktop.bottom - rDesktop.top)

```

Validate the fields before using them.

### Flags OOB

Several locations use the Flags field as an iterator with no bounds checking, causing access violations. This is often due to iterating over `SMARTMAX_DISPLAY_ENTRY.cells` which is 32 elements in size. As such, iterators must be limited to `min(flags, ARRAY_SIZE(flags))`.

This one is demonstrated with the CrashOnWindowMove PoC.
```c
18001df1e  if (arg_10 u< pDisplayEntry->Flags || arg_10 == 0xffffffff)

18001df77      __builtin_memcpy(&rTaskbar, &pDisplayEntry->cells[zx.q(arg_10)], 0x10)
...
18001e03e      arg_10 = zx.d(SmartMax_FindNearestMonitorEdge(arg3, rdx_5))
18001e067      __builtin_memcpy(&rTaskbar, &pDisplayEntry->cells[zx.q(arg_10)], 0x10)
```

Here is another instance.

```c
18001ee7b  while (zx.d(idx) u< rax_4->Flags)
...
18001eeb5      if (PtInRect(lprc: &rax_4->cells[zx.q(idx)], pt: pt_1) != 0)

```

Here is another instance (arg2 is  SMARTMAX_DISPLAY_ENTRY->Flags).

```c
18001433b  for (int32_t i = 0; i u< *arg2; i += 1)
180014346      if (var_3c != 0)
180014346          break
180014346      
18001435e      POINT pt
18001435e      pt.x = arg_18.x
18001435e      pt.y = arg_18.y
18001435e      
180014371      if (PtInRect(lprc: &arg2[zx.q(i) * 4 + 7], pt) == 0)

```

Here is another instance.

```c
18001355f for (int32_t i = 0; i u< pDisplayEntry->Flags; i += 1)
18001358d     RECT tempRect
18001358d     __builtin_memcpy(dest: &tempRect, 
18001358d         src: &pDisplayEntry->cells[zx.q(i)], count: 0x10)
```

### Process Name OOB Write

`szProcessName` is `wchar [0x28]`, however, there are a couple of locations where writes can go out of bounds.

```c
180020a18  SmartMax_wcsncpy_s_truncate(
180020a18      dst: &rax_20->Apps[sx.q(i_1)].szProcessName, 
180020a18      cchDst: 0x104, src: arg1)

```

Here is another instance (similar code, different address).

```c
180020abb  SmartMax_wcsncpy_s_truncate(
180020abb      dst: &rax_20->Apps[sx.q(i_2)].szProcessName, 
180020abb      cchDst: 0x104, src: arg1)
```

### Window Manipulation

Window positions can be modified and moved anywhere. This gives low-integrity users the ability to move, or hide, windows controlled by high-integrity processes. Back in the day this was considered an issue due to weaker browser sandboxes and UAC bypasses, no idea if it is anymore but here it is regardless.

```c
180015c5c  if (GetMaxToGridRect(&wp.rcNormalPosition, &rNew, 1) != 0)
180015cac      SetWindowPos(hWnd: arg1, hWndInsertAfter: nullptr, X: rNew.left, 
180015cac          Y: rNew.top, cx: rNew.right - rNew.left, 
180015cac          cy: rNew.bottom - rNew.top, uFlags: SWP_SHOWWINDOW)

```