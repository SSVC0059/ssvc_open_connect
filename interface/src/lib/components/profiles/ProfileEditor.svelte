<script lang="ts">
    import type { Profile } from '$lib/types/ssvc';

    let { profile, onSave, onCancel } = $props();

</script>

<div class="profile-editor">
    <h3>Редактирование профиля: {profile.name}</h3>
    <p>Здесь будет форма для редактирования параметров профиля.</p>

    <div class="editor-actions">
        <button class="btn btn-success" onclick={onSave}>Сохранить</button>
        <button class="btn btn-back" onclick={onCancel}>Отмена</button>
    </div>
</div>

<style>
    .profile-editor {
        padding: 1rem;
    }
    .editor-actions {
        margin-top: 1rem;
        display: flex;
        gap: 0.5rem;
    }
</style>
