// Use the same root/belonging-group elimination sequence as native off-screen
// casualties, while leader HP is still positive and attacker credit is available.
static bool ValidDefeatChildren(void* group)
{
    const int count = At<int>(group, 0x58);
    if (count < 0 || count > MaxGroups)
        return false;
    return count == 0 || Readable(At<int*>(group, 0x50), static_cast<size_t>(count) * sizeof(int));
}

static void DefeatOffscreenFormation(void* group)
{
    int children[MaxGroups];
    const int count = At<int>(group, 0x58);
    if (count > 0)
        memcpy(children, At<int*>(group, 0x50), static_cast<size_t>(count) * sizeof(int));
    const int root = At<int>(group, 0x3C);
    void* storage = GroupStorage;
    At<uint8_t>(group, 0x1D4) = 1; // Native off-screen defeat marker, before casualty callbacks.
    NativeGroupDefeat(group, 1);
    for (int index = 0; index < count && GroupStorage == storage; ++index)
    {
        void* child = Group(children[index]);
        if (child && child != group && At<int>(child, 0x48) == root && GroupAlive(child))
            NativeGroupDefeat(child, 1);
    }
    if (GroupStorage == storage && Group(root) == group)
        At<int>(group, 0x1D0) = -1; // Native resolver consumes last-attacker credit once.
}
