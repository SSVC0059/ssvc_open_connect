<script lang="ts">
    import type { Profile } from '$lib/types/ssvc';

    let { profile } = $props();
</script>

<div class="profile-viewer">
    <h3>Настройки профиля: {profile.name}</h3>
    <p>ID: {profile.id}</p>
    <p>This is where the detailed settings for "{profile.name}" would be displayed.</p>
</div>

<style>
    .profile-viewer {
        padding: 1rem;
    }
</style>
