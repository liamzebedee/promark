One hit test system. One render traversal. Events bubble.



❯ yeah. I guess I wonder if they make it better? ie an underlying runtime of abstract objects and click surfaces etc. rather than having coordinate translation systems betwen everything. same with UI

● You're touching on a real architectural insight. The "coordinate translation everywhere" approach is how most things start, but it gets messy fast.

  Two Paradigms

  What you probably have now:
  Screen coords → Viewport offset → Document coords → Layout coords
       ↓
    Toolbar (screen space, separate system)
       ↓
    Manual hit testing everywhere

  Scene graph / unified object model:
  Root
   ├─ Toolbar (fixed, doesn't scroll)
   └─ Viewport (scrollable)
        └─ Document
             ├─ Paragraph (knows its bounds, hittable)
             ├─ Image (knows its bounds, hittable)
             └─ ...

  Every node has:
  - transform (position/scale relative to parent)
  - bounds (local size)
  - hitTest(localPoint) → bool
  - children[]

  Hit testing becomes: walk tree, apply inverse transforms, ask each node.

  Why It's Better
  ┌─────────────────────────────┬────────────────────────────────┬──────────────────────────────────┐
  │           Problem           │     Coordinate Translation     │           Scene Graph            │
  ├─────────────────────────────┼────────────────────────────────┼──────────────────────────────────┤
  │ Adding new object types     │ Edit hit test logic everywhere │ Just add a node type             │
  ├─────────────────────────────┼────────────────────────────────┼──────────────────────────────────┤
  │ Nested scrolling            │ Manual offset stacking         │ Transforms compose automatically │
  ├─────────────────────────────┼────────────────────────────────┼──────────────────────────────────┤
  │ Click on overlapping things │ Manual z-order checks          │ Tree order defines it            │
  ├─────────────────────────────┼────────────────────────────────┼──────────────────────────────────┤
  │ UI vs content               │ Two separate systems           │ Same system, different subtrees  │
  └─────────────────────────────┴────────────────────────────────┴──────────────────────────────────┘
  This Is Basically What Browsers Do

  The DOM is exactly this - a tree of boxes with:
  - Inherited transforms (CSS transform, scroll offset)
  - Event bubbling (click hits deepest node, bubbles up)
  - Layout as a tree operation

  And game engines (Unity, Godot) - everything is a node with a transform.

  For Your Editor

  You could unify toolbar + document into one tree:
  RootNode
   ├─ ToolbarNode (y=0, fixed)
   │    ├─ ButtonNode (bold)
   │    └─ ButtonNode (italic)
   └─ ScrollContainerNode
        └─ DocumentNode
             ├─ ParagraphNode
             └─ ImageNode

  One hit test system. One render traversal. Events bubble.
